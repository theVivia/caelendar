package com.caelendar.app

import com.caelendar.app.data.CalendarRepository
import java.io.BufferedReader
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.ServerSocket
import kotlin.concurrent.thread

/** 局域网分享：手机开一个只读 HTTP 页，同一 WiFi 下用电脑/别的手机浏览器看未来 7 天计划（桌面 share.cpp 的对应功能）。 */
object ShareServer {
    private const val PORT = 8787
    @Volatile
    private var serverSo: ServerSocket? = null
    @Volatile
    private var worker: Thread? = null

    fun isRunning(): Boolean = serverSo?.isClosed == false

    fun localIp(): String {
        return try {
            NetworkInterface.getNetworkInterfaces().toList()
                .flatMap { it.inetAddresses.toList() }
                .firstOrNull { !it.isLoopbackAddress && it is Inet4Address }
                ?.hostAddress ?: "127.0.0.1"
        } catch (_: Exception) {
            "127.0.0.1"
        }
    }

    fun url(): String = "http://${localIp()}:$PORT/"

    fun start(repo: CalendarRepository): Boolean {
        if (isRunning()) return true
        if (worker != null) return true
        // ServerSocket 必须在线程里创建（主线程创建会抛 NetworkOnMainThreadException）
        worker = thread(isDaemon = true, name = "caelendar-share") {
            val ss = try {
                ServerSocket(PORT)
            } catch (_: Exception) {
                worker = null
                return@thread
            }
            serverSo = ss
            try {
                while (!ss.isClosed) {
                    try {
                        ss.accept().use { c ->
                            val reader = c.getInputStream().bufferedReader()
                            reader.readLine()
                            try {
                                val page = buildPage(repo.load())
                                val bytes = page.toByteArray(Charsets.UTF_8)
                                val header = "HTTP/1.1 200 OK\r\n" +
                                    "Content-Type: text/html; charset=utf-8\r\n" +
                                    "Content-Length: ${bytes.size}\r\n" +
                                    "Cache-Control: no-store\r\n" +
                                    "Connection: close\r\n\r\n"
                                c.getOutputStream().write(header.toByteArray(Charsets.UTF_8))
                                c.getOutputStream().write(bytes)
                                c.getOutputStream().flush()
                            } catch (_: Exception) {
                            }
                        }
                    } catch (_: Exception) {
                    }
                }
            } catch (_: Exception) {
            } finally {
                try { ss.close() } catch (_: Exception) {}
                serverSo = null
                worker = null
            }
        }
        return true
    }

    fun stop() {
        try {
            serverSo?.close()
        } catch (_: Exception) {
        }
        serverSo = null
        worker = null
    }

    private fun buildPage(store: com.caelendar.app.data.CtStore): String {
        val today = java.time.LocalDate.now()
        val sb = StringBuilder()
        sb.append("<!DOCTYPE html><html><head><meta charset='utf-8'>")
        sb.append("<meta name='viewport' content='width=device-width,initial-scale=1'>")
        sb.append("<title>星历 · 未来 7 天</title>")
        sb.append("<style>body{font-family:-apple-system,'Segoe UI',sans-serif;background:#0b0d14;color:#f7f9ff;padding:18px;margin:0}")
        sb.append("h1{color:#e3c177;font-size:20px}.day{margin:14px 0;padding:10px 12px;background:#141824;border-radius:10px}")
        sb.append(".dh{color:#e3c177;font-weight:600;margin-bottom:4px}.ev{padding:3px 0;font-size:14px}")
        sb.append(".t{color:#8892a6;font-size:12px;margin-right:8px}.loc{color:#8892a6;font-size:12px}")
        sb.append(".empty{color:#5b6478;font-size:13px}</style></head><body>")
        sb.append("<h1>✦ 星历 · 未来 7 天</h1>")
        sb.append("<div class='t'>${today} 更新 · 只读页面</div>")
        val names = listOf("周一", "周二", "周三", "周四", "周五", "周六", "周日")
        for (i in 0..6) {
            val d = today.plusDays(i.toLong())
            val evs = ExpandEvents(store.events, d, d)
                .filter { it.start.startsWith(d.toString()) }
                .sortedBy { it.start }
            sb.append("<div class='day'><div class='dh'>${d.monthValue}月${d.dayOfMonth}日 ${names[d.dayOfWeek.value - 1]}")
            if (evs.isNotEmpty()) sb.append(" · ${evs.size} 项")
            sb.append("</div>")
            if (evs.isEmpty()) sb.append("<div class='empty'>无日程</div>")
            for (e in evs) {
                val t = if (e.start.length >= 16) e.start.substring(11, 16) else ""
                sb.append("<div class='ev'><span class='t'>$t</span>${e.title.ifEmpty { "未命名" }}")
                if (e.location.isNotEmpty()) sb.append(" <span class='loc'>· ${e.location}</span>")
                sb.append("</div>")
            }
            sb.append("</div>")
        }
        sb.append("</body></html>")
        return sb.toString()
    }
}
