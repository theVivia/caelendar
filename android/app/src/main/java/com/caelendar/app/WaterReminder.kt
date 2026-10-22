package com.caelendar.app

import android.app.AlarmManager
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import com.caelendar.app.data.CalendarRepository
import java.time.LocalDate
import java.time.LocalDateTime
import java.time.ZoneId

private const val WATER_CHANNEL = "caelendar_water"
private const val WATER_PREFS = "caelendar_water_reminder"

/** 喝水提醒：每天 08:00–22:30 每 N 分钟提醒；通知可一键 +1 杯（与桌面 health.cpp 同款数据 water）。 */
object WaterReminder {
    private fun prefs(ctx: Context) = ctx.getSharedPreferences(WATER_PREFS, Context.MODE_PRIVATE)

    fun isEnabled(ctx: Context) = prefs(ctx).getBoolean("enabled", true)

    fun intervalMin(ctx: Context) = prefs(ctx).getInt("interval", 45).coerceAtLeast(15)

    fun setEnabled(ctx: Context, on: Boolean) {
        prefs(ctx).edit().putBoolean("enabled", on).apply()
        if (on) scheduleNext(ctx) else cancel(ctx)
    }

    /** 推算下一次提醒（当天 08:00–22:30 内按间隔对齐）。 */
    fun nextWaterTime(now: LocalDateTime, interval: Int): LocalDateTime? {
        val day = now.toLocalDate()
        val startMin = 8 * 60
        val endMin = 22 * 60 + 30
        var mins = now.hour * 60 + now.minute
        var step = ((mins / interval) + 1) * interval
        if (step <= endMin) {
            var t = day.atStartOfDay().plusMinutes(step.toLong())
            if (t.isAfter(now)) return t
            t = t.plusMinutes(interval.toLong())
            return if (t.hour * 60 + t.minute <= endMin) t else day.plusDays(1).atTime(8, 0)
        }
        return day.plusDays(1).atTime(8, 0).takeIf { now.hour * 60 + now.minute >= startMin } ?: day.atTime(8, 0)
    }

    fun scheduleNext(ctx: Context) {
        if (!isEnabled(ctx)) return
        val at = nextWaterTime(LocalDateTime.now(), intervalMin(ctx)) ?: return
        val pi = PendingIntent.getBroadcast(
            ctx, 9001, Intent(ctx, WaterReminderReceiver::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val am = ctx.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val trigger = at.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli()
        try {
            am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, trigger, pi)
        } catch (_: SecurityException) {
            am.set(AlarmManager.RTC_WAKEUP, trigger, pi)
        }
    }

    fun cancel(ctx: Context) {
        val pi = PendingIntent.getBroadcast(
            ctx, 9001, Intent(ctx, WaterReminderReceiver::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        (ctx.getSystemService(Context.ALARM_SERVICE) as AlarmManager).cancel(pi)
    }

    fun notifyNow(ctx: Context) {
        val repo = CalendarRepository(ctx.filesDir)
        val store = repo.load()
        val today = LocalDate.now().toString()
        val cups = store.waterMap()[today] ?: 0
        val nm = ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(NotificationChannel(WATER_CHANNEL, "喝水提醒", NotificationManager.IMPORTANCE_DEFAULT))
        }
        val addPi = PendingIntent.getBroadcast(
            ctx, 9002,
            Intent(ctx, WaterReminderReceiver::class.java).putExtra("add", true),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val openPi = PendingIntent.getActivity(
            ctx, 9003,
            Intent(ctx, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val n = NotificationCompat.Builder(ctx, WATER_CHANNEL)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle("💧 该喝水了")
            .setContentText(if (cups >= 8) "今天 $cups/8 杯，喝够了" else "今天 $cups/8 杯，来一杯")
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setAutoCancel(true)
            .setContentIntent(openPi)
            .addAction(0, "+1 杯", addPi)
            .build()
        try {
            nm.notify(9001, n)
        } catch (_: SecurityException) {
        }
    }
}

class WaterReminderReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.getBooleanExtra("add", false)) {
            val repo = CalendarRepository(context.filesDir)
            val store = repo.load()
            val today = LocalDate.now().toString()
            val cups = store.waterMap()[today] ?: 0
            if (cups < 8) repo.save(store.withWater(today, cups + 1))
            (context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).cancel(9001)
            WaterReminder.notifyNow(context)
        } else {
            WaterReminder.notifyNow(context)
        }
        WaterReminder.scheduleNext(context)
    }
}
