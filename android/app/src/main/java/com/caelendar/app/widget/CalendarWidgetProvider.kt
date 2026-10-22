package com.caelendar.app.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews
import com.caelendar.app.MainActivity
import com.caelendar.app.R
import com.caelendar.app.data.CalendarRepository
import java.time.LocalDate

/** 桌面小组件 v1：今日日程数 + 下一场倒计时/时间。 */
class CalendarWidgetProvider : AppWidgetProvider() {
    override fun onUpdate(context: Context, appWidgetManager: AppWidgetManager, appWidgetIds: IntArray) {
        val repo = CalendarRepository(context.filesDir)
        val store = repo.load()
        val today = LocalDate.now().toString()
        val todayEvents = store.events.filter { !it.done && it.start.startsWith(today) }
        val now = java.time.LocalDateTime.now().toString().take(16)
        val next = store.events
            .filter { !it.done && it.start.length >= 16 && it.start > now }
            .minByOrNull { it.start }

        val mode = context.getSharedPreferences("caelendar_widget", Context.MODE_PRIVATE)
            .getString("mode", "today") ?: "today"
        val nowLocal = java.time.LocalDateTime.now()
        val weekEnd = java.time.LocalDate.now().plusDays(7).toString()

        for (id in appWidgetIds) {
            val views = RemoteViews(context.packageName, R.layout.widget_calendar)
            views.setTextViewText(R.id.widget_title, "星历")
            when (mode) {
                "week" -> {
                    val weekCount = store.events.count {
                        !it.done && it.start.length >= 10 && it.start.take(10) >= today && it.start.take(10) < weekEnd
                    }
                    views.setTextViewText(R.id.widget_today_count, "本周 $weekCount 项")
                }
                "clock" -> {
                    views.setTextViewText(R.id.widget_title, nowLocal.format(java.time.format.DateTimeFormatter.ofPattern("HH:mm")))
                    views.setTextViewText(R.id.widget_today_count, "今天 ${todayEvents.size} 项")
                }
                else -> views.setTextViewText(R.id.widget_today_count, "今天 ${todayEvents.size} 项")
            }
            if (next != null) {
                val date = next.start.substring(5, 10).replace('-', '/')
                val time = next.start.substring(11, 16)
                views.setTextViewText(R.id.widget_next, "下一场 $date $time · ${next.title}")
            } else {
                views.setTextViewText(R.id.widget_next, "接下来没有日程")
            }
            val open = Intent(context, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            }
            val pi = PendingIntent.getActivity(
                context, 100, open,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            views.setOnClickPendingIntent(R.id.widget_root, pi)
            appWidgetManager.updateAppWidget(id, views)
        }
    }
}
