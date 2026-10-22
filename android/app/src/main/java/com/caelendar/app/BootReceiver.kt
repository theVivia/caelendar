package com.caelendar.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/** 开机后重新排一次提醒（AlarmManager 不跨重启保留）。 */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            ReminderScheduler.scheduleNext(context)
        }
    }
}
