package com.caelendar.app

import android.app.AlarmManager
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.caelendar.app.data.CalendarRepository
import com.caelendar.app.data.CtEvent
import java.time.LocalDateTime
import java.time.ZoneId

/** 通知操作按钮：完成 / 稍后提醒。 */
class NotificationActionReceiver : BroadcastReceiver() {
    companion object {
        const val ACTION_DONE = "com.caelendar.app.NOTIFY_DONE"
        const val ACTION_SNOOZE = "com.caelendar.app.NOTIFY_SNOOZE"
    }

    override fun onReceive(context: Context, intent: Intent) {
        val id = intent.getStringExtra("eventId") ?: ""
        val notifyId = intent.getIntExtra("notifyId", 1)
        when (intent.action) {
            ACTION_DONE -> {
                if (id.isNotEmpty()) {
                    val repo = CalendarRepository(context.filesDir)
                    val store = repo.load()
                    val list = store.events.toMutableList()
                    val idx = list.indexOfFirst { it.id == id }
                    if (idx >= 0) {
                        list[idx] = list[idx].copy(done = true)
                        repo.save(store.copy(events = list))
                    }
                }
                val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                nm.cancel(notifyId)
            }
            ACTION_SNOOZE -> {
                // 10 分钟后再弹同一提醒
                val title = intent.getStringExtra("title") ?: "日程提醒"
                val start = intent.getStringExtra("start") ?: ""
                val location = intent.getStringExtra("location") ?: ""
                val date = intent.getStringExtra("date") ?: ""
                val again = Intent(context, ReminderReceiver::class.java).apply {
                    putExtra("eventId", id)
                    putExtra("title", title)
                    putExtra("start", start)
                    putExtra("location", location)
                    putExtra("date", date)
                }
                val pi = PendingIntent.getBroadcast(
                    context, 12, again,
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
                val am = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
                val at = LocalDateTime.now().plusMinutes(10)
                    .atZone(ZoneId.systemDefault()).toInstant().toEpochMilli()
                try {
                    am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi)
                } catch (_: SecurityException) {
                    am.set(AlarmManager.RTC_WAKEUP, at, pi)
                }
                val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                nm.cancel(notifyId)
            }
        }
    }
}
