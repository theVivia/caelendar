package com.caelendar.app

import android.Manifest
import android.app.AlarmManager
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.caelendar.app.data.CalendarRepository
import com.caelendar.app.data.CtEvent
import java.time.LocalDateTime
import java.time.ZoneId

/** 提前多少分钟提醒（v1 固定 10 分钟）。 */
private const val LEAD_MINUTES = 10L
private const val CHANNEL_ID = "caelendar_reminders"

fun ensureNotificationPermission(activity: android.app.Activity) {
    if (Build.VERSION.SDK_INT >= 33) {
        val granted = ContextCompat.checkSelfPermission(activity, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
        if (!granted) {
            activity.requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 100)
        }
    }
}

/** 调度未来最近的若干条提醒（支持事件多条 alarms：每条提前量各一条）。 */
object ReminderScheduler {
    private data class Candidate(val at: LocalDateTime, val ev: CtEvent, val lead: Int)

    fun scheduleNext(context: Context) {
        val repo = CalendarRepository(context.filesDir)
        val store = repo.load()
        val now = LocalDateTime.now()
        val cands = mutableListOf<Candidate>()
        for (e in store.events) {
            if (e.allDay || e.done || e.start.isEmpty()) continue
            val start = CtEvent.parseDT(e.start) ?: continue
            val leads = if (e.alarms.isEmpty()) listOf(LEAD_MINUTES.toInt()) else e.alarms
            for (lead in leads) {
                val remindAt = start.minusMinutes(lead.coerceAtLeast(0).toLong())
                if (remindAt.isAfter(now)) cands.add(Candidate(remindAt, e, lead))
            }
        }
        cands.sortBy { it.at }
        val am = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        for ((i, c) in cands.take(8).withIndex()) {
            val ev = c.ev
            val intent = Intent(context, ReminderReceiver::class.java).apply {
                putExtra("title", ev.title)
                putExtra("start", ev.start)
                putExtra("location", ev.location)
                putExtra("date", ev.start.take(10))
                putExtra("lead", c.lead)
                putExtra("eventId", ev.id)
            }
            val pi = PendingIntent.getBroadcast(
                context, 100 + i, intent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            val triggerAt = c.at.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli()
            try {
                am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerAt, pi)
            } catch (_: SecurityException) {
                am.set(AlarmManager.RTC_WAKEUP, triggerAt, pi)
            }
        }
    }
}

class ReminderReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val title = intent.getStringExtra("title") ?: "日程提醒"
        val start = intent.getStringExtra("start") ?: ""
        val location = intent.getStringExtra("location") ?: ""
        val time = if (start.length >= 16) start.substring(11, 16) else ""
        val text = buildString {
            append(time)
            if (location.isNotEmpty()) append(" · ").append(location)
        }
        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (Build.VERSION.SDK_INT >= 26) {
            val ch = NotificationChannel(CHANNEL_ID, "日程提醒", NotificationManager.IMPORTANCE_HIGH)
            nm.createNotificationChannel(ch)
        }
        val open = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            putExtra("date", intent.getStringExtra("date") ?: "")
            putExtra("editEventId", intent.getStringExtra("eventId") ?: "")
        }
        val pi = PendingIntent.getActivity(
            context, 2, open,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val n = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle("⏰ $title")
            .setContentText(text)
            .setAutoCancel(true)
            .setContentIntent(pi)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .build()
        try {
            nm.notify(1, n)
        } catch (_: SecurityException) {
            // 没有通知权限时静默
        }
        // 继续调度下一条
        ReminderScheduler.scheduleNext(context)
    }
}
