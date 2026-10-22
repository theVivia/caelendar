package com.caelendar.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import kotlinx.coroutines.withTimeoutOrNull
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.caelendar.app.data.CalendarRepository
import com.caelendar.app.data.GradeRec
import com.caelendar.app.data.GpaRepository
import com.caelendar.app.data.CtEvent
import com.caelendar.app.data.CtStore
import com.caelendar.app.data.CtTag
import com.caelendar.app.data.SyncClient
import com.caelendar.app.data.SyncPrefs
import com.caelendar.app.data.CtTemplate
import com.caelendar.app.data.TemplateStore
import com.caelendar.app.data.IcsClient
import com.caelendar.app.data.Merge
import com.caelendar.app.data.HabitsStore
import com.caelendar.app.data.LunarCalendar
import com.caelendar.app.data.SolarTerms
import com.caelendar.app.data.SmartParse
import com.caelendar.app.data.CtHabit
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import android.content.Intent
import android.widget.Toast
import java.io.File
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import java.time.DayOfWeek
import java.time.LocalDate
import org.json.JSONArray
import org.json.JSONObject
import java.time.LocalDateTime
import java.time.LocalTime
import java.time.YearMonth
import java.time.format.DateTimeFormatter
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.detectDragGestures
import com.caelendar.app.data.BackupCrypto
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.isCtrlPressed
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.key.key
import com.caelendar.app.data.BotClient
import com.caelendar.app.data.CtRecur
import com.caelendar.app.data.CtTombstone
import com.caelendar.app.data.startsBetween
import com.caelendar.app.data.Holiday

private object ThemeState {
    private val prefs = mutableMapOf<String, String>()

    var dark by mutableStateOf(true)
    var accentIndex by mutableStateOf(0)
    var highContrast by mutableStateOf(false)
    var fontScale by mutableStateOf(1f)
    var customAccentHex by mutableStateOf("")

    data class Preset(val name: String, val accent: Int, val dark: Boolean)
    val presets = listOf(
        Preset("金色午夜", 0, true),
        Preset("极简蓝", 1, true),
        Preset("森林绿", 2, true),
        Preset("浅色纸张", 0, false)
    )

    val accents = listOf(
        Color(0xFFE3C177), Color(0xFF6EA8FF), Color(0xFF7ED9A0), Color(0xFFC084FC),
        Color(0xFFF472B6), Color(0xFF4FD1C5), Color(0xFFFFB454), Color(0xFFFF7A85)
    )
    val accentNames = listOf("金", "蓝", "绿", "紫", "粉", "青", "橙", "红")
    val accent: Color
        get() {
            val hex = customAccentHex.trim()
            if (hex.length == 7 && hex.startsWith("#")) {
                try {
                    return Color(android.graphics.Color.parseColor(hex))
                } catch (_: Exception) {
                }
            }
            return accents[accentIndex.coerceIn(0, accents.size - 1)]
        }

    fun load(ctx: android.content.Context) {
        val sp = ctx.getSharedPreferences("caelendar_theme", android.content.Context.MODE_PRIVATE)
        dark = sp.getBoolean("dark", true)
        accentIndex = sp.getInt("accent", 0)
        highContrast = sp.getBoolean("highContrast", false)
        fontScale = sp.getFloat("fontScale", 1f)
        customAccentHex = sp.getString("customAccent", "") ?: ""
    }

    fun save(ctx: android.content.Context) {
        ctx.getSharedPreferences("caelendar_theme", android.content.Context.MODE_PRIVATE).edit()
            .putBoolean("dark", dark).putInt("accent", accentIndex)
            .putBoolean("highContrast", highContrast).putFloat("fontScale", fontScale)
            .putString("customAccent", customAccentHex).apply()
    }
}

private val Bg: Color get() = if (ThemeState.dark) Color(0xFF07080D) else Color(0xFFF7F4EC)
private val CardColor: Color get() = if (ThemeState.dark) Color(0xFF141824) else Color(0xFFFFFFFF)
private val Accent: Color get() = ThemeState.accent
private val TextMain: Color get() = if (ThemeState.dark) Color(0xFFF7F9FF) else Color(0xFF111420)
private val TextDim: Color get() = when {
    ThemeState.dark && ThemeState.highContrast -> Color(0xFFD6DCEA)
    ThemeState.dark -> Color(0xFF9AA3B8)
    ThemeState.highContrast -> Color(0xFF33384A)
    else -> Color(0xFF5A6070)
}

class MainActivity : ComponentActivity() {
    private val deepLinkDate = mutableStateOf<String?>(null)
    private val shortcutAction = mutableStateOf<String?>(null)
    private val shareText = mutableStateOf<String?>(null)
    private val editEventId = mutableStateOf<String?>(null)

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        deepLinkDate.value = intent.getStringExtra("date")
        shortcutAction.value = intent.getStringExtra("shortcut")
        editEventId.value = intent.getStringExtra("editEventId")
        handleShare(intent)
    }

    override fun onResume() {
        super.onResume()
        // 回到应用时重排提醒：新建/修改日程后也能刷新下一场提醒
        ReminderScheduler.scheduleNext(this)
        WaterReminder.scheduleNext(this)
    }

    private fun handleShare(intent: android.content.Intent?) {
        if (intent == null) return
        if (intent.action == android.content.Intent.ACTION_SEND) {
            val text = intent.getStringExtra(android.content.Intent.EXTRA_TEXT)
            if (!text.isNullOrBlank()) shareText.value = text
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val repo = CalendarRepository(filesDir).also { r ->
        val prefs = getSharedPreferences("caelendar_storage", android.content.Context.MODE_PRIVATE)
        if (prefs.getString("engine", "json") == "sqlite") {
            r.sqlite = com.caelendar.app.data.SqliteStore(this)
        }
    }
        val gpaRepo = GpaRepository(filesDir)
        ensureNotificationPermission(this)
        ReminderScheduler.scheduleNext(this)
        deepLinkDate.value = intent?.getStringExtra("date")
        shortcutAction.value = intent?.getStringExtra("shortcut")
        editEventId.value = intent?.getStringExtra("editEventId")
        handleShare(intent)
        ThemeState.load(this)
        setTheme(R.style.Theme_Caelendar_Main)
        setContent {
            androidx.compose.runtime.CompositionLocalProvider(
                androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(
                    androidx.compose.ui.platform.LocalDensity.current.density,
                    androidx.compose.ui.platform.LocalDensity.current.fontScale * ThemeState.fontScale
                )
            ) {
            MaterialTheme(
                colorScheme = if (ThemeState.dark) {
                    darkColorScheme(background = Bg, surface = CardColor, primary = Accent)
                } else {
                    lightColorScheme(background = Bg, surface = CardColor, primary = Accent)
                }
            ) {
                CaelendarApp(
                    repo, gpaRepo, deepLinkDate.value,
                    shortcutAction = shortcutAction.value,
                    shareText = shareText.value,
                    editEventId = editEventId.value,
                    onConsumeShortcut = { shortcutAction.value = null },
                    onConsumeShare = { shareText.value = null },
                    onConsumeEdit = { editEventId.value = null }
                ) { deepLinkDate.value = null }
            }
            }
        }
    }
}

private enum class Tab { Month, List, Tasks, Gpa, Settings }

@OptIn(androidx.compose.ui.ExperimentalComposeUiApi::class)
@Composable
private fun CaelendarApp(
    repo: CalendarRepository,
    gpaRepo: GpaRepository,
    deepLinkDate: String?,
    shortcutAction: String? = null,
    shareText: String? = null,
    editEventId: String? = null,
    onConsumeShortcut: () -> Unit = {},
    onConsumeShare: () -> Unit = {},
    onConsumeEdit: () -> Unit = {},
    onDeepLink: () -> Unit
) {
    var store by remember { mutableStateOf(repo.load()) }
    var tab by remember { mutableStateOf(Tab.Month) }
    var month by remember { mutableStateOf(YearMonth.now()) }
    var selectedDay by remember { mutableStateOf(LocalDate.now()) }
    LaunchedEffect(deepLinkDate) {
        val d = deepLinkDate
        if (!d.isNullOrEmpty()) {
            try {
                val day = LocalDate.parse(d)
                selectedDay = day
                month = YearMonth.from(day)
                tab = Tab.Month
            } catch (_: Exception) {
                // 忽略格式错误
            }
            onDeepLink()
        }
    }
    val ctx = LocalContext.current
    val templateStore = remember { TemplateStore(ctx) }
    var showTemplates by remember { mutableStateOf(false) }
    var showBulk by remember { mutableStateOf(false) }
    var showSearch by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<CtEvent?>(null) }
    var viewMode by remember { mutableStateOf(0) }  // 0=月 1=周 2=日
    var editing by remember { mutableStateOf<CtEvent?>(null) }
    gTags = store.tags
    var undoStack by remember { mutableStateOf(mutableListOf<String>()) }
    var redoStack by remember { mutableStateOf(mutableListOf<String>()) }
    var clipboardEvent by remember { mutableStateOf<CtEvent?>(null) }
    var contextEvent by remember { mutableStateOf<CtEvent?>(null) }
    var dayDetailDay by remember { mutableStateOf<LocalDate?>(null) }
    var batchSelectDay by remember { mutableStateOf<LocalDate?>(null) }
    var batchMode by remember { mutableStateOf(false) }
    val batchSel = remember { mutableStateListOf<String>() }
    var showBatchTagPicker by remember { mutableStateOf(false) }

    fun snapshot() {
        if (undoStack.size > 30) undoStack = undoStack.drop(1).toMutableList()
        undoStack = (undoStack + store.toJsonString()).toMutableList()
        redoStack = mutableListOf()
    }

    fun undo() {
        val last = undoStack.lastOrNull() ?: return
        undoStack = undoStack.dropLast(1).toMutableList()
        redoStack = (redoStack + store.toJsonString()).toMutableList()
        val restored = CtStore.fromJsonString(last)
        store = restored
        repo.save(restored)
    }

    fun moveEventDate(e: CtEvent, deltaDays: Int) {
        if (deltaDays == 0) return
        val sd = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull() ?: return
        val ed = runCatching { LocalDate.parse(e.end.take(10)) }.getOrNull() ?: sd
        val ns = sd.plusDays(deltaDays.toLong()).toString() + e.start.substring(10)
        val ne = ed.plusDays(deltaDays.toLong()).toString() + e.end.substring(10)
        val nowStr = java.time.LocalDateTime.now()
            .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
        snapshot()
        val list = store.events.toMutableList()
        if (e.instanceDate.isNotEmpty()) {
            val idx = list.indexOfFirst { it.id == e.id }
            if (idx >= 0) {
                val series = list[idx]
                val ov = e.copy(
                    id = "${series.id}#${e.instanceDate}",
                    recurId = e.instanceDate,
                    instanceDate = "",
                    start = ns,
                    end = ne,
                    updated = nowStr
                )
                val keeps = series.overridesList().filter {
                    (it.recurId.ifEmpty { it.start }) != e.instanceDate
                }
                list[idx] = series.withOverrides(keeps + ov, nowStr)
                store = store.copy(events = list)
                repo.save(store)
                android.widget.Toast.makeText(ctx, "已把这次课移到 ${ns.take(10)}", android.widget.Toast.LENGTH_SHORT).show()
            }
        } else {
            val idx = list.indexOfFirst { it.id == e.id }
            if (idx >= 0) {
                list[idx] = list[idx].copy(start = ns, end = ne, updated = nowStr)
                store = store.copy(events = list)
                repo.save(store)
                android.widget.Toast.makeText(ctx, "已移到 ${ns.take(10)}", android.widget.Toast.LENGTH_SHORT).show()
            }
        }
    }

    fun redo() {
        val next = redoStack.lastOrNull() ?: return
        redoStack = redoStack.dropLast(1).toMutableList()
        undoStack = (undoStack + store.toJsonString()).toMutableList()
        val restored = CtStore.fromJsonString(next)
        store = restored
        repo.save(restored)
    }

    val kbdMod = Modifier.onPreviewKeyEvent { ev ->
        if (ev.type == androidx.compose.ui.input.key.KeyEventType.KeyDown && ev.isCtrlPressed) {
            when (ev.key) {
                androidx.compose.ui.input.key.Key.Z -> { undo(); true }
                androidx.compose.ui.input.key.Key.Y -> { redo(); true }
                androidx.compose.ui.input.key.Key.N -> {
                    editing = CtEvent(
                        id = UUID.randomUUID().toString(),
                        start = "${selectedDay}T09:00",
                        end = "${selectedDay}T10:00"
                    )
                    true
                }
                androidx.compose.ui.input.key.Key.F -> { showSearch = true; true }
                androidx.compose.ui.input.key.Key.T -> {
                    selectedDay = LocalDate.now()
                    month = YearMonth.now()
                    true
                }
                else -> false
            }
        } else false
    }
    val sharedCtx = LocalContext.current

    LaunchedEffect(shortcutAction) {
        if (!shortcutAction.isNullOrEmpty()) {
            if (shortcutAction == "new") {
                editing = CtEvent(
                    id = UUID.randomUUID().toString(),
                    start = "${selectedDay}T09:00",
                    end = "${selectedDay}T10:00"
                )
            } else if (shortcutAction == "today") {
                selectedDay = LocalDate.now()
                month = YearMonth.now()
                tab = Tab.Month
            }
            onConsumeShortcut()
        }
    }
    LaunchedEffect(editEventId) {
        val id = editEventId
        if (!id.isNullOrEmpty()) {
            val ev = store.events.firstOrNull { it.id == id }
            if (ev != null) {
                editing = ev
                val d = runCatching { LocalDate.parse(ev.start.take(10)) }.getOrNull()
                if (d != null) {
                    selectedDay = d
                    month = YearMonth.from(d)
                }
            }
            onConsumeEdit()
        }
    }
    LaunchedEffect(shareText) {
        val text = shareText
        if (!text.isNullOrEmpty()) {
            if (text.trimStart().startsWith("BEGIN:VCALENDAR")) {
                val imported = IcsClient.parse(text)
                if (imported.isNotEmpty()) {
                    val list = store.events.toMutableList()
                    list.addAll(imported)
                    store = store.copy(events = list)
                    repo.save(store)
                }
            } else {
                val firstLine = text.lines().firstOrNull { it.isNotBlank() }?.take(60) ?: "分享的日程"
                editing = CtEvent(
                    id = UUID.randomUUID().toString(),
                    title = firstLine,
                    desc = text.take(2000),
                    start = "${selectedDay}T09:00",
                    end = "${selectedDay}T10:00"
                )
            }
            onConsumeShare()
        }
    }

    fun rescheduleEvent(e: CtEvent, minutes: Int) {
        if (e.start.length < 16 || e.end.length < 16) return
        snapshot()
        val fmt = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm")
        val st = try { java.time.LocalDateTime.parse(e.start, fmt) } catch (_: Exception) { return }
        val en = try { java.time.LocalDateTime.parse(e.end, fmt) } catch (_: Exception) { return }
        val ns = st.plusMinutes(minutes.toLong())
        val ne = en.plusMinutes(minutes.toLong())
        val now = java.time.LocalDateTime.now().format(fmt)
        val updated = e.copy(start = ns.format(fmt), end = ne.format(fmt), updated = now)
        val list = store.events.toMutableList()
        val idx = list.indexOfFirst { it.id == e.id }
        if (idx >= 0) {
            list[idx] = updated
            store = store.copy(events = list)
            repo.save(store)
        }
    }

    fun persist() {
        repo.save(store)
        store = store.copy(events = store.events.toMutableList())
    }

    Scaffold(containerColor = Bg) { padding ->
        Column(Modifier.padding(padding).fillMaxSize().then(kbdMod)) {
            Row(
                Modifier.fillMaxWidth().background(Color(0xFF0B0D14))
                    .horizontalScroll(rememberScrollState())
                    .padding(horizontal = 8.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                androidx.compose.foundation.Image(
                    painter = androidx.compose.ui.res.painterResource(R.drawable.logo_brand),
                    contentDescription = "星历",
                    modifier = Modifier.size(32.dp)
                )
                Spacer(Modifier.width(4.dp))
                TextButton(
                    onClick = { showSearch = true },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                    modifier = Modifier.semantics { contentDescription = "搜索日程" }
                ) { Text("搜索", color = TextDim, fontSize = 10.sp) }
                Spacer(Modifier.width(2.dp))
                TabButton("预览", tab == Tab.Month) { tab = Tab.Month }
                TabButton("列表", tab == Tab.List) { tab = Tab.List }
                TabButton("任务", tab == Tab.Tasks) { tab = Tab.Tasks }
                TabButton("GPA", tab == Tab.Gpa) { tab = Tab.Gpa }
                TabButton("设置", tab == Tab.Settings) { tab = Tab.Settings }

            }
            when (tab) {
                Tab.Month -> Column(Modifier.fillMaxSize()) {
                    ViewModeBar(viewMode) { viewMode = it }
                    if (undoStack.isNotEmpty() || redoStack.isNotEmpty()) {
                        Row(
                            Modifier.fillMaxWidth().padding(horizontal = 10.dp),
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            if (undoStack.isNotEmpty()) {
                                TextButton(onClick = { undo() }) { Text("↶ 撤销", color = TextDim, fontSize = 11.sp) }
                            }
                            if (redoStack.isNotEmpty()) {
                                TextButton(onClick = { redo() }) { Text("↷ 重做", color = TextDim, fontSize = 11.sp) }
                            }
                        }
                    }
                    if (viewMode != 0) {
                        Row(
                            Modifier.fillMaxWidth().padding(horizontal = 8.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            TextButton(onClick = {
                                when (viewMode) {
                                    1 -> selectedDay = selectedDay.minusDays(7)
                                    2 -> selectedDay = selectedDay.minusDays(1)
                                    else -> month = month.minusYears(1)
                                }
                            }) { Text("‹", color = Accent, fontSize = 22.sp) }
                            Text(
                                when (viewMode) {
                                    1 -> {
                                        val ws = DaysOfWeek(selectedDay)
                                        "${ws.first().monthValue}/${ws.first().dayOfMonth} - ${ws.last().monthValue}/${ws.last().dayOfMonth}"
                                    }
                                    2 -> "${selectedDay.year}年${selectedDay.monthValue}月${selectedDay.dayOfMonth}日 ${WeekNames[selectedDay.dayOfWeek.value - 1]}"
                                    else -> "${month.year} 年"
                                },
                                color = TextMain, fontSize = 15.sp, fontWeight = FontWeight.Bold
                            )
                            TextButton(onClick = {
                                when (viewMode) {
                                    1 -> selectedDay = selectedDay.plusDays(7)
                                    2 -> selectedDay = selectedDay.plusDays(1)
                                    else -> month = month.plusYears(1)
                                }
                            }) { Text("›", color = Accent, fontSize = 22.sp) }
                        }
                    }
                    val swipeMod = Modifier.pointerInput(viewMode, selectedDay) {
                        var drag = 0f
                        detectHorizontalDragGestures(
                            onDragEnd = {
                                if (drag > 80f) {
                                    when (viewMode) {
                                        1 -> selectedDay = selectedDay.minusDays(7)
                                        2 -> selectedDay = selectedDay.minusDays(1)
                                    }
                                } else if (drag < -80f) {
                                    when (viewMode) {
                                        1 -> selectedDay = selectedDay.plusDays(7)
                                        2 -> selectedDay = selectedDay.plusDays(1)
                                    }
                                }
                                drag = 0f
                            }
                        ) { _, delta -> drag += delta }
                    }
                    Box(Modifier.fillMaxSize().then(if (viewMode == 1 || viewMode == 2) swipeMod else Modifier)) {
                    when (viewMode) {
                        1 -> TimelineScreen(
                            store = store,
                            days = DaysOfWeek(selectedDay),
                            selectedDay = selectedDay,
                            onSelectDay = { selectedDay = it },
                            onEdit = { editing = it },
                            onNew = { d, h ->
                                editing = CtEvent(
                                    id = UUID.randomUUID().toString(),
                                    start = "${d}T%02d:00".format(h),
                                    end = "${d}T%02d:00".format((h + 1) % 24)
                                )
                            },
                            onReschedule = { e, minutes -> rescheduleEvent(e, minutes) },
                            onMoveDate = { e, delta -> moveEventDate(e, delta) }
                        )
                        2 -> TimelineScreen(
                            store = store,
                            days = listOf(selectedDay),
                            selectedDay = selectedDay,
                            onSelectDay = { selectedDay = it },
                            onEdit = { editing = it },
                            onNew = { d, h ->
                                editing = CtEvent(
                                    id = UUID.randomUUID().toString(),
                                    start = "${d}T%02d:00".format(h),
                                    end = "${d}T%02d:00".format((h + 1) % 24)
                                )
                            },
                            onReschedule = { e, minutes -> rescheduleEvent(e, minutes) }
                        )
                        3 -> YearView(store, month.year) { m ->
                            month = YearMonth.of(month.year, m)
                            viewMode = 0
                        }
                        else -> MonthScreen(
                            store = store,
                            month = month,
                            selectedDay = selectedDay,
                            onMonthChange = { month = it },
                            onSelectDay = { selectedDay = it },
                            onEdit = { editing = it },
                            onCreate = { editing = CtEvent(id = UUID.randomUUID().toString(), start = "${selectedDay}T09:00", end = "${selectedDay}T10:00") },
                            onTemplates = { showTemplates = true },
                            onBulk = { showBulk = true },
                            water = store.waterMap(),
                            onWater = { d, c ->
                                val next = store.withWater(d, c)
                                store = next
                                repo.save(next)
                            },
                            onLongPressEvent = { contextEvent = it },
                            onLongPressDay = { d ->
                                val cb = clipboardEvent
                                if (cb != null) {
                                    snapshot()
                                    val t1 = if (cb.start.length >= 16) cb.start.substring(11, 16) else "09:00"
                                    val t2 = if (cb.end.length >= 16) cb.end.substring(11, 16) else "10:00"
                                    val pasted = cb.copy(
                                        id = UUID.randomUUID().toString(),
                                        start = "${d}T$t1",
                                        end = "${d}T$t2",
                                        created = "",
                                        updated = ""
                                    )
                                    store = store.copy(events = (store.events + pasted).toMutableList())
                                    persist()
                                    android.widget.Toast.makeText(ctx, "已粘贴到 $d", android.widget.Toast.LENGTH_SHORT).show()
                                }
                            },
                            onDayDetail = { dayDetailDay = it },
                            hasClipboard = clipboardEvent != null,
                            onMulti = { batchSelectDay = selectedDay },
                            batchMode = batchMode,
                            batchSelected = batchSel.toList(),
                            onEnterBatch = { e ->
                                batchMode = true
                                batchSel.clear()
                                batchSel.add("${e.id}@${e.start}")
                            },
                            onToggleBatch = { e ->
                                val key = "${e.id}@${e.start}"
                                if (key in batchSel) batchSel.remove(key) else batchSel.add(key)
                            },
                            onExitBatch = {
                                batchMode = false
                                batchSel.clear()
                            },
                            onBatchAction = { action ->
                                if (batchSel.isEmpty()) {
                                    Toast.makeText(ctx, "先选中要操作的日程", Toast.LENGTH_SHORT).show()
                                } else if (action == "tag") {
                                    showBatchTagPicker = true
                                } else {
                                    snapshot()
                                    val nowStr = java.time.LocalDateTime.now()
                                        .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
                                    val list = store.events.toMutableList()
                                    val ids = batchSel.map { it.substringBefore("@") }.toSet()
                                    if (action == "done") {
                                        var n = 0
                                        for (i in list.indices) {
                                            if (list[i].id in ids) {
                                                list[i] = list[i].copy(done = true, updated = nowStr)
                                                n++
                                            }
                                        }
                                        store = store.copy(events = list)
                                        Toast.makeText(ctx, "已标记完成 $n 项", Toast.LENGTH_SHORT).show()
                                    } else if (action == "delete") {
                                        val tombs = store.tombstones.toMutableList()
                                        for (id in ids) tombs.add(com.caelendar.app.data.CtTombstone(id, nowStr))
                                        val remain = list.filter { it.id !in ids }
                                        store = store.copy(
                                            events = remain.toMutableList(),
                                            tombstones = tombs.takeLast(300).toMutableList()
                                        )
                                        Toast.makeText(ctx, "已删除 ${ids.size} 项", Toast.LENGTH_SHORT).show()
                                    }
                                    persist()
                                    batchMode = false
                                    batchSel.clear()
                                }
                            }
                        )
                    }
                    }
                }
                Tab.List -> ListScreen(store, onEdit = { editing = it })
                Tab.Tasks -> DdlScreen(store) { e ->
                    val list = store.events.toMutableList()
                    val idx = list.indexOfFirst { it.id == e.id }
                    if (idx >= 0) list[idx] = list[idx].copy(done = true)
                    store = store.copy(events = list)
                    repo.save(store)
                }
                Tab.Gpa -> GpaScreen(gpaRepo)
                Tab.Settings -> SettingsScreen(repo, gpaRepo, store) { store = it }
            }
        }
    }

    if (showBulk) {
        BulkAddDialog(
            defaultDate = selectedDay,
            onCreate = { list ->
                val next = store.events.toMutableList()
                next.addAll(list)
                store = store.copy(events = next)
                repo.save(store)
                showBulk = false
            },
            onDismiss = { showBulk = false }
        )
    }
    if (showTemplates) {
        val templates = templateStore.load()
        TemplateDialog(
            templates = templates,
            onUse = { t ->
                showTemplates = false
                editing = CtEvent(
                    id = UUID.randomUUID().toString(),
                    title = t.title,
                    location = t.location,
                    start = "${selectedDay}T${t.startTime}",
                    end = "${selectedDay}T${t.endTime}",
                    priority = t.priority,
                    color = t.color,
                    alarms = if (t.alarm < 0) mutableListOf() else mutableListOf(t.alarm),
                    recurJson = when (t.recurFreq) {
                        1 -> "FREQ=DAILY"
                        2 -> "FREQ=WEEKLY;INTERVAL=1"
                        3 -> "FREQ=MONTHLY"
                        else -> ""
                    }
                )
            },
            onDelete = { t -> templateStore.save(templates.filter { it != t }) },
            onDismiss = { showTemplates = false }
        )
    }
    editing?.let { ev ->
            EditDialog(
                event = ev,
                allTags = store.tags,
                existing = store.events,
                onSaveTemplate = { t -> templateStore.save(templateStore.load() + t) },
                onDelete = { deleteTarget = ev },
                onDismiss = { editing = null },
            onSave = { savedRaw ->
                snapshot()
                val nowStr = java.time.LocalDateTime.now()
                    .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
                val list = store.events.toMutableList()
                if (savedRaw.instanceDate.isNotEmpty()) {
                    // 重复系列的单次实例：不覆盖原系列，写成 overrides 子记录
                    val idx = list.indexOfFirst { it.id == savedRaw.id }
                    if (idx >= 0) {
                        val series = list[idx]
                        val ov = savedRaw.copy(
                            id = "${series.id}#${savedRaw.instanceDate}",
                            recurId = savedRaw.instanceDate,
                            instanceDate = "",
                            updated = nowStr,
                            created = savedRaw.created.ifEmpty { nowStr },
                            deleted = false
                        )
                        val keeps = series.overridesList().filter {
                            (it.recurId.ifEmpty { it.start }) != savedRaw.instanceDate
                        }
                        list[idx] = series.withOverrides(keeps + ov, nowStr)
                    }
                } else {
                    val saved = savedRaw.copy(
                        updated = nowStr,
                        created = savedRaw.created.ifEmpty { nowStr },
                        instanceDate = ""
                    )
                    val idx = list.indexOfFirst { it.id == saved.id }
                    if (idx >= 0) list[idx] = saved else list.add(saved)
                }
                store = store.copy(events = list)
                persist()
                editing = null
            }
        )
    }
    if (showSearch) {
        SearchDialog(
            store = store,
            onJump = { d ->
                selectedDay = d
                month = YearMonth.from(d)
                tab = Tab.Month
                viewMode = 0
                showSearch = false
            },
            onEdit = { e ->
                editing = e
                showSearch = false
            },
            onCommand = { cmd ->
                showSearch = false
                when {
                    cmd == ">new" -> editing = CtEvent(
                        id = UUID.randomUUID().toString(),
                        start = "${selectedDay}T09:00",
                        end = "${selectedDay}T10:00"
                    )
                    cmd == ">today" -> {
                        selectedDay = LocalDate.now()
                        month = YearMonth.now()
                        tab = Tab.Month
                        viewMode = 0
                    }
                    cmd == ">month" -> { tab = Tab.Month; viewMode = 0 }
                    cmd == ">week" -> { tab = Tab.Month; viewMode = 1 }
                    cmd == ">day" -> { tab = Tab.Month; viewMode = 2 }
                    cmd == ">year" -> { tab = Tab.Month; viewMode = 3 }
                    cmd == ">settings" -> tab = Tab.Settings
                    cmd == ">gpa" -> tab = Tab.Gpa
                    cmd == ">list" -> tab = Tab.List
                    cmd == ">tasks" -> tab = Tab.Tasks
                    cmd.startsWith(">date ") -> {
                        val d = runCatching { LocalDate.parse(cmd.removePrefix(">date ").trim()) }.getOrNull()
                        if (d != null) {
                            selectedDay = d
                            month = YearMonth.from(d)
                            tab = Tab.Month
                            viewMode = 0
                        }
                    }
                }
            },
            onDismiss = { showSearch = false }
        )
    }
    deleteTarget?.let { target ->
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text("删除日程", color = TextMain, fontWeight = FontWeight.Bold) },
            text = {
                Text(
                    if (target.instanceDate.isNotEmpty()) {
                        "取消「${target.title.ifEmpty { "未命名" }}」这一次（${target.start.take(16).replace("T", " ")}）？该系列其它周次保留。"
                    } else {
                        "确定删除「${target.title.ifEmpty { "未命名" }}」？删除会同步到其它设备。"
                    },
                    color = TextMain
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    val now = java.time.LocalDateTime.now()
                        .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
                    snapshot()
                    if (target.instanceDate.isNotEmpty()) {
                        // 取消这一次课：给系列写 deleted override，系列其余周次保留
                        val list = store.events.toMutableList()
                        val idx = list.indexOfFirst { it.id == target.id }
                        if (idx >= 0) {
                            val series = list[idx]
                            val ov = target.copy(
                                id = "${series.id}#${target.instanceDate}",
                                recurId = target.instanceDate,
                                instanceDate = "",
                                deleted = true,
                                updated = now
                            )
                            val keeps = series.overridesList().filter {
                                (it.recurId.ifEmpty { it.start }) != target.instanceDate
                            }
                            list[idx] = series.withOverrides(keeps + ov, now)
                            store = store.copy(events = list)
                            persist()
                        }
                    } else {
                        val remain = store.events.filter { it.id != target.id }
                        val tombs = (store.tombstones + CtTombstone(target.id, now)).takeLast(300)
                        store = store.copy(events = remain.toMutableList(), tombstones = tombs.toMutableList())
                        persist()
                    }
                    deleteTarget = null
                    editing = null
                }) { Text("删除", color = Color(0xFFFF6B7A)) }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) { Text("取消", color = TextDim) }
            }
        )
    }
    contextEvent?.let { ev ->
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { contextEvent = null },
            title = { Text(ev.title.ifEmpty { "未命名" }, color = TextMain, fontWeight = FontWeight.Bold) },
            text = {
                Text(
                    ev.start.take(16).replace("T", " ") +
                        (if (ev.end.length >= 16) " – " + ev.end.substring(11, 16) else "") +
                        (if (ev.location.isNotEmpty()) "  ·  " + ev.location else ""),
                    color = TextDim
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    clipboardEvent = ev.copy(created = "", updated = "")
                    android.widget.Toast.makeText(ctx, "已复制，长按任意日期格子即可粘贴", android.widget.Toast.LENGTH_SHORT).show()
                    contextEvent = null
                }) { Text("复制", color = Accent) }
            },
            dismissButton = {
                Row {
                    TextButton(onClick = { contextEvent = null; editing = ev }) { Text("编辑", color = TextDim) }
                    TextButton(onClick = { contextEvent = null; deleteTarget = ev }) { Text("删除", color = Color(0xFFFF6B7A)) }
                    TextButton(onClick = { contextEvent = null }) { Text("取消", color = TextDim) }
                }
            }
        )
    }
    if (showBatchTagPicker) {
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { showBatchTagPicker = false },
            title = { Text("给 ${batchSel.size} 项加标签", color = TextMain, fontWeight = FontWeight.Bold) },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    for (t in store.tags) {
                        TextButton(onClick = {
                            snapshot()
                            val nowStr = java.time.LocalDateTime.now()
                                .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
                            val ids = batchSel.map { it.substringBefore("@") }.toSet()
                            val list = store.events.toMutableList()
                            var n = 0
                            for (i in list.indices) {
                                if (list[i].id in ids) {
                                    val merged = (list[i].tags + t.id).distinct().toMutableList()
                                    list[i] = list[i].copy(tags = merged, updated = nowStr)
                                    n++
                                }
                            }
                            store = store.copy(events = list)
                            persist()
                            Toast.makeText(ctx, "已给 $n 项加标签「${t.name}」", Toast.LENGTH_SHORT).show()
                            showBatchTagPicker = false
                            batchMode = false
                            batchSel.clear()
                        }) {
                            Text("${t.name}（${store.events.count { e -> e.tags.contains(t.id) }} 项）", color = TextMain, fontSize = 14.sp)
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showBatchTagPicker = false }) { Text("取消", color = TextDim) }
            }
        )
    }
    batchSelectDay?.let { day ->
        val dayEvs = ExpandEvents(store.events, day, day)
            .filter { it.start.startsWith(day.toString()) }
            .sortedBy { it.start }
        BatchDialog(
            day = day,
            events = dayEvs,
            tags = store.tags,
            onApply = { selected, action, tagIds ->
                snapshot()
                val nowStr = java.time.LocalDateTime.now()
                    .format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm"))
                val list = store.events.toMutableList()
                when (action) {
                    "done" -> {
                        var n = 0
                        for (i in list.indices) {
                            if (list[i].id in selected) { list[i] = list[i].copy(done = true, updated = nowStr); n++ }
                        }
                        store = store.copy(events = list)
                        Toast.makeText(ctx, "已标记完成 $n 项", Toast.LENGTH_SHORT).show()
                    }
                    "delete" -> {
                        val tombs = store.tombstones.toMutableList()
                        var n = 0
                        for (id in selected) {
                            tombs.add(com.caelendar.app.data.CtTombstone(id, nowStr))
                            n++
                        }
                        val remain = list.filter { it.id !in selected }
                        store = store.copy(events = remain.toMutableList(), tombstones = tombs.takeLast(300).toMutableList())
                        Toast.makeText(ctx, "已删除 $n 项", Toast.LENGTH_SHORT).show()
                    }
                    "tag" -> {
                        var n = 0
                        for (i in list.indices) {
                            if (list[i].id in selected) {
                                val merged = (list[i].tags + tagIds).distinct().toMutableList()
                                list[i] = list[i].copy(tags = merged, updated = nowStr)
                                n++
                            }
                        }
                        store = store.copy(events = list)
                        Toast.makeText(ctx, "已给 $n 项加标签", Toast.LENGTH_SHORT).show()
                    }
                }
                persist()
                batchSelectDay = null
            },
            onDismiss = { batchSelectDay = null }
        )
    }
    dayDetailDay?.let { day ->
        val evs = ExpandEvents(store.events, day, day)
            .filter { it.start.startsWith(day.toString()) }
            .sortedBy { it.start }
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { dayDetailDay = null },
            title = { Text("${day} 详情（${evs.size} 项）", color = TextMain, fontWeight = FontWeight.Bold) },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    if (evs.isEmpty()) Text("这一天还没有日程", color = TextDim)
                    for (e in evs) {
                        Text(
                            (if (e.allDay) "全天" else e.start.substringAfter('T') + " – " + e.end.substringAfter('T')) +
                                (if (e.done) "  ✓" else ""),
                            color = TextDim, fontSize = 11.sp
                        )
                        Text(e.title.ifEmpty { "未命名" }, color = TextMain, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                        if (e.location.isNotEmpty()) Text("📍 ${e.location}", color = TextDim, fontSize = 12.sp)
                        if (e.desc.isNotEmpty()) Text(e.desc, color = TextDim, fontSize = 12.sp, maxLines = 2)
                        Spacer(Modifier.height(8.dp))
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { dayDetailDay = null; editing = CtEvent(id = UUID.randomUUID().toString(), start = "${day}T09:00", end = "${day}T10:00") }) {
                    Text("新建", color = Accent)
                }
            },
            dismissButton = { TextButton(onClick = { dayDetailDay = null }) { Text("关闭", color = TextDim) } }
        )
    }
}

@Composable
private fun TextImportDialog(
    onImport: (List<CtEvent>) -> Unit,
    onDismiss: () -> Unit
) {
    val ctx = LocalContext.current
    var text by remember { mutableStateOf("") }
    var msg by remember { mutableStateOf("") }

    fun parseLines(raw: String): List<CtEvent> {
        val out = mutableListOf<CtEvent>()
        val trimmed = raw.trim()
        if (trimmed.startsWith("BEGIN:VCALENDAR")) {
            return try { IcsClient.parse(trimmed) } catch (_: Exception) { emptyList() }
        }
        val fmt = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm")
        for (line in trimmed.lines()) {
            val l = line.trim()
            if (l.isEmpty() || l.startsWith("#")) continue
            val parts = l.split(",", "，", "\t")
            if (parts.size < 2) continue
            val title = parts[0].trim()
            if (title.isEmpty() || title == "标题") continue
            val dateStr = parts[1].trim()
            var start = ""
            var end = ""
            if (dateStr.length >= 16) {
                start = dateStr.replace(" ", "T").take(16)
            } else if (dateStr.length >= 10) {
                val t1 = if (parts.size >= 3 && parts[2].contains(":")) parts[2].trim() else "09:00"
                val t2 = if (parts.size >= 4 && parts[3].contains(":")) parts[3].trim() else "10:00"
                start = "${dateStr.take(10)}T${t1.take(5)}"
                end = "${dateStr.take(10)}T${t2.take(5)}"
            }
            if (start.length < 16) continue
            if (end.isEmpty()) {
                end = try {
                    java.time.LocalDateTime.parse(start, fmt).plusHours(1).format(fmt)
                } catch (_: Exception) {
                    start
                }
            }
            val location = parts.getOrNull(4)?.trim() ?: ""
            out.add(
                CtEvent(
                    id = java.util.UUID.randomUUID().toString(),
                    title = title,
                    start = start,
                    end = end,
                    location = location
                )
            )
        }
        return out
    }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(14.dp).heightIn(max = 580.dp)) {
                Text("批量导入", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text(
                    "每行一条：标题,日期,开始,结束,地点（也支持 ICS 文本）。例：高数,2026-11-01,09:00,10:00,A101",
                    color = TextDim, fontSize = 10.sp
                )
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = {
                        try {
                            val cm = ctx.getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                            val clip = cm.primaryClip?.getItemAt(0)?.text?.toString() ?: ""
                            if (clip.isNotEmpty()) {
                                text = clip
                                msg = "已粘贴 ${clip.lines().size} 行"
                            } else {
                                msg = "剪贴板为空"
                            }
                        } catch (e: Exception) {
                            msg = "读取剪贴板失败：${e.message}"
                        }
                    }) { Text("粘贴剪贴板", color = Accent) }
                    TextButton(onClick = {
                        val parsed = parseLines(text)
                        if (parsed.isEmpty()) {
                            msg = "没有解析出日程"
                        } else {
                            onImport(parsed)
                        }
                    }) { Text("解析并导入", color = Accent) }
                    TextButton(onClick = {
                        val parsed = parseTimetableLines(text)
                        if (parsed.isEmpty()) {
                            msg = "教务表格式：课程名,周一,3-4节,教室"
                        } else {
                            onImport(parsed)
                        }
                    }) { Text("按教务表导入", color = Accent) }
                }
                if (msg.isNotEmpty()) {
                    Text(msg, color = TextDim, fontSize = 11.sp)
                }
                OutlinedTextField(
                    value = text, onValueChange = { text = it },
                    label = { Text("粘贴文本 / CSV / ICS") },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 200.dp).padding(top = 4.dp)
                )
                val preview = if (text.isBlank()) emptyList() else parseLines(text)
                Text(
                    if (text.isBlank()) "" else "可导入 ${preview.size} 条日程",
                    color = if (preview.isEmpty()) Color(0xFFFF6B7A) else Accent,
                    fontSize = 12.sp
                )
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim) }
                }
            }
        }
    }
}

private fun buildBusyFreeText(events: List<CtEvent>): String {
    val sb = StringBuilder()
    sb.append("我的空闲时间（星历）\n")
    val today = java.time.LocalDate.now()
    for (i in 0 until 7) {
        val d = today.plusDays(i.toLong())
        val dayEvents = events.filter { e ->
            !e.done && e.start.length >= 16 && e.start.startsWith(d.toString())
        }.sortedBy { it.start }
        val busy = mutableListOf<Pair<Int, Int>>()
        for (e in dayEvents) {
            val sh = e.start.substring(11, 13).toIntOrNull() ?: continue
            val sm = e.start.substring(14, 16).toIntOrNull() ?: 0
            val eh = e.end.substring(11, 13).toIntOrNull() ?: (sh + 1)
            val em = e.end.substring(14, 16).toIntOrNull() ?: 0
            busy.add((sh * 60 + sm) to (eh * 60 + em))
        }
        val free = mutableListOf<Pair<Int, Int>>()
        var cur = 9 * 60
        for ((bs, be) in busy) {
            if (bs > cur) free.add(cur to minOf(bs, 21 * 60))
            cur = maxOf(cur, be)
        }
        if (cur < 21 * 60) free.add(cur to 21 * 60)
        val label = "${d.monthValue}/${d.dayOfMonth} " + listOf("一", "二", "三", "四", "五", "六", "日")[d.dayOfWeek.value - 1]
        if (free.isEmpty()) {
            sb.append("$label：无空闲\n")
        } else {
            sb.append(label).append("：").append(
                free.filter { it.second - it.first >= 30 }
                    .take(3)
                    .joinToString("，") { "%02d:%02d-%02d:%02d".format(it.first / 60, it.first % 60, it.second / 60, it.second % 60) }
            ).append("\n")
        }
    }
    return sb.toString()
}

@Composable
private fun BotDialog(
    store: CtStore,
    onDismiss: () -> Unit
) {
    val ctx = LocalContext.current
    val sp = remember {
        ctx.getSharedPreferences("caelendar_bot", android.content.Context.MODE_PRIVATE)
    }
    var token by remember { mutableStateOf(sp.getString("token", "") ?: "") }
    var chatId by remember { mutableStateOf(sp.getString("chatId", "") ?: "") }
    var status by remember { mutableStateOf("") }
    val scope = rememberCoroutineScope()

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(14.dp).heightIn(max = 560.dp)) {
                Text("Telegram 机器人", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text("填 BotFather 给的 token 和你的 chatId。桌面版同款协议，Android 端手动拉取命令。", color = TextDim, fontSize = 10.sp)
                OutlinedTextField(
                    value = token, onValueChange = { token = it },
                    label = { Text("Bot Token") }, singleLine = true,
                    modifier = Modifier.fillMaxWidth().padding(top = 4.dp)
                )
                OutlinedTextField(
                    value = chatId, onValueChange = { chatId = it },
                    label = { Text("Chat ID") }, singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
                if (status.isNotEmpty()) {
                    Text(status, color = Accent, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = {
                        sp.edit().putString("token", token.trim()).putString("chatId", chatId.trim()).apply()
                        status = "已保存"
                    }) { Text("保存", color = Accent) }
                    TextButton(onClick = {
                        if (token.isBlank()) {
                            status = "先填 token"
                        } else {
                            scope.launch(Dispatchers.IO) {
                                val name = BotClient.getMe(token.trim())
                                withContext(Dispatchers.Main) {
                                    status = if (name != null) "连接成功：@$name" else "连接失败（检查 token）"
                                }
                            }
                        }
                    }) { Text("测试连接", color = TextDim) }
                    TextButton(onClick = {
                        val today = java.time.LocalDate.now().toString()
                        val weekEnd = java.time.LocalDate.now().plusDays(7).toString()
                        val todayEvents = store.events.filter { !it.done && it.start.startsWith(today) }.sortedBy { it.start }
                        val weekEvents = store.events.filter {
                            !it.done && it.start.take(10) >= today && it.start.take(10) < weekEnd
                        }.sortedBy { it.start }
                        scope.launch(Dispatchers.IO) {
                            val ok = BotClient.sendMessage(
                                token.trim(), chatId.trim(),
                                "今天 ${todayEvents.size} 项：\n" + todayEvents.take(10).joinToString("\n") {
                                    "· ${it.start.substring(11, 16)} ${it.title}"
                                }
                            )
                            withContext(Dispatchers.Main) {
                                status = if (ok) "已发送今日日程到 Telegram" else "发送失败（检查 chatId / 是否先给 bot 发过消息）"
                            }
                        }
                    }) { Text("发送今日", color = TextDim) }
                    TextButton(onClick = {
                        scope.launch(Dispatchers.IO) {
                            val msgs = BotClient.getUpdates(token.trim())
                            var replied = 0
                            val today = java.time.LocalDate.now()
                            for ((chat, text) in msgs) {
                                val reply = when {
                                    text.startsWith("/today") -> {
                                        val evs = store.events.filter { !it.done && it.start.startsWith(today.toString()) }
                                            .sortedBy { it.start }
                                        "今天 ${evs.size} 项：\n" + evs.take(10).joinToString("\n") { "· ${it.start.substring(11, 16)} ${it.title}" }
                                    }
                                    text.startsWith("/week") -> {
                                        val end = today.plusDays(7).toString()
                                        val evs = store.events.filter {
                                            !it.done && it.start.take(10) >= today.toString() && it.start.take(10) < end
                                        }.sortedBy { it.start }
                                        "未来 7 天 ${evs.size} 项：\n" + evs.take(15).joinToString("\n") { "· ${it.start.substring(5, 10)} ${it.start.substring(11, 16)} ${it.title}" }
                                    }
                                    else -> "可用命令：/today 今日 · /week 未来 7 天"
                                }
                                if (chat.isNotEmpty() && BotClient.sendMessage(token.trim(), chat, reply)) replied++
                            }
                            withContext(Dispatchers.Main) {
                                status = "拉取 ${msgs.size} 条命令，回复 $replied 条"
                            }
                        }
                    }) { Text("拉取命令并回复", color = TextDim) }
                }
                Text(
                    "提示：先给 bot 发一句任意消息，chatId 才能收到回复；token 只存本机。",
                    color = TextDim, fontSize = 10.sp
                )
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim) }
                }
            }
        }
    }
}

/** 教务表行解析：课程名,周一,3-4节,教室 → 16 周课程事件。 */
/** 生成未来 7 天学习计划 HTML（桌面 printplan.cpp 的轻量替代，可直接分享/打印）。 */
private fun buildPlanHtml(store: CtStore): String {
    val today = LocalDate.now()
    val sb = StringBuilder()
    sb.append("<!DOCTYPE html><html><head><meta charset='utf-8'>")
    sb.append("<title>星历 · 学习计划</title>")
    sb.append("<style>body{font-family:-apple-system,'Segoe UI',sans-serif;background:#0b0d14;color:#f7f9ff;padding:24px}")
    sb.append("h1{color:#e3c177;font-size:22px}table{border-collapse:collapse;width:100%;margin-top:8px}")
    sb.append("th,td{border:1px solid #2a2f3d;padding:6px 8px;font-size:14px;vertical-align:top}")
    sb.append("th{background:#141824;color:#e3c177;width:130px;text-align:left}")
    sb.append(".ev{margin:2px 0;padding:2px 6px;border-radius:4px;background:#1d2436}</style></head><body>")
    sb.append("<h1>✦ 星历 · 未来 7 天学习计划</h1>")
    sb.append("<div style='color:#8892a6;font-size:12px'>生成于 ${today}</div><table>")
    for (i in 0..6) {
        val d = today.plusDays(i.toLong())
        val evs = ExpandEvents(store.events, d, d)
            .filter { it.start.startsWith(d.toString()) }
            .sortedBy { it.start }
        sb.append("<tr><th>${d.monthValue}月${d.dayOfMonth}日 ${listOf("周一","周二","周三","周四","周五","周六","周日")[d.dayOfWeek.value - 1]}</th><td>")
        if (evs.isEmpty()) sb.append("<span style='color:#5b6478'>无日程</span>")
        for (e in evs) {
            val t = if (e.start.length >= 16) e.start.substring(11, 16) + " " else ""
            sb.append("<div class='ev'>$t${e.title.ifEmpty { "未命名" }}")
            if (e.location.isNotEmpty()) sb.append(" <span style='color:#8892a6'>· ${e.location}</span>")
            sb.append("</div>")
        }
        sb.append("</td></tr>")
    }
    sb.append("</table></body></html>")
    return sb.toString()
}

private fun parseTimetableLines(raw: String): List<CtEvent> {
    val weekdays = mapOf(
        "周一" to java.time.DayOfWeek.MONDAY, "星期二" to java.time.DayOfWeek.TUESDAY,
        "周二" to java.time.DayOfWeek.TUESDAY, "星期三" to java.time.DayOfWeek.WEDNESDAY,
        "周三" to java.time.DayOfWeek.WEDNESDAY, "星期四" to java.time.DayOfWeek.THURSDAY,
        "周四" to java.time.DayOfWeek.THURSDAY, "星期五" to java.time.DayOfWeek.FRIDAY,
        "周五" to java.time.DayOfWeek.FRIDAY, "星期六" to java.time.DayOfWeek.SATURDAY,
        "周六" to java.time.DayOfWeek.SATURDAY, "星期日" to java.time.DayOfWeek.SUNDAY,
        "周日" to java.time.DayOfWeek.SUNDAY
    )
    val sectionTime = mapOf(
        "1-2" to ("08:00" to "09:35"), "3-4" to ("10:05" to "11:40"),
        "5-6" to ("14:00" to "15:35"), "7-8" to ("16:05" to "17:40"),
        "9-10" to ("19:00" to "20:35"), "11-12" to ("20:45" to "22:20")
    )
    val out = mutableListOf<CtEvent>()
    val monday = java.time.LocalDate.now().with(java.time.DayOfWeek.MONDAY)
    for (line in raw.lines()) {
        val l = line.trim()
        if (l.isEmpty() || l.startsWith("#")) continue
        val parts = l.split(",", "，").map { it.trim() }.filter { it.isNotEmpty() }
        if (parts.size < 3) continue
        val title = parts[0]
        val wd = weekdays[parts[1]] ?: continue
        val section = parts[2].replace("节", "").trim()
        val (t1, t2) = sectionTime[section] ?: continue
        val location = parts.getOrNull(3) ?: ""
        var d = monday
        while (d.dayOfWeek != wd) d = d.plusDays(1)
        for (w in 0 until 16) {
            val day = d.plusWeeks(w.toLong())
            out.add(
                CtEvent(
                    id = java.util.UUID.randomUUID().toString(),
                    title = title,
                    location = location,
                    start = "${day}T$t1",
                    end = "${day}T$t2"
                )
            )
        }
    }
    return out
}

@Composable
private fun SubscriptionDialog(
    store: CtStore,
    onImported: (List<CtEvent>) -> Unit,
    onDismiss: () -> Unit
) {
    val ctx = LocalContext.current
    var url by remember { mutableStateOf("") }
    var urls by remember {
        mutableStateOf(
            ctx.getSharedPreferences("caelendar_subs", android.content.Context.MODE_PRIVATE)
                .getStringSet("urls", emptySet())?.toList()?.sorted() ?: emptyList()
        )
    }
    var status by remember { mutableStateOf("") }
    val scope = rememberCoroutineScope()

    fun saveUrls(list: List<String>) {
        urls = list
        ctx.getSharedPreferences("caelendar_subs", android.content.Context.MODE_PRIVATE)
            .edit().putStringSet("urls", list.toSet()).apply()
    }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(14.dp).heightIn(max = 540.dp)) {
                Text("订阅 ICS", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text("填 ICS 订阅地址，点「立即同步」拉取并合并到本地", color = TextDim, fontSize = 10.sp)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = url, onValueChange = { url = it },
                        label = { Text("https://.../calendar.ics") }, singleLine = true,
                        modifier = Modifier.weight(1f)
                    )
                    TextButton(onClick = {
                        val u = url.trim()
                        if (u.startsWith("http")) {
                            saveUrls((urls + u).distinct())
                            url = ""
                            status = "已添加订阅"
                        }
                    }) { Text("添加", color = Accent) }
                }
                if (status.isNotEmpty()) {
                    Text(status, color = TextDim, fontSize = 11.sp)
                }
                LazyColumn(Modifier.weight(1f, fill = false)) {
                    items(urls) { u ->
                        Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                            Text(u, color = TextMain, fontSize = 11.sp, modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                            TextButton(onClick = { saveUrls(urls - u) }) { Text("删除", color = Color(0xFFFF6B7A), fontSize = 11.sp) }
                        }
                    }
                    if (urls.isEmpty()) {
                        item { Text("还没有订阅", color = TextDim, fontSize = 12.sp) }
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = {
                        if (urls.isEmpty()) {
                            status = "先添加订阅地址"
                        } else {
                            status = "同步中…"
                            scope.launch(Dispatchers.IO) {
                                val existingIds = store.events.map { it.id }.toHashSet()
                                val existingUids = store.events.map { it.uid }.filter { it.isNotEmpty() }.toHashSet()
                                val added = mutableListOf<CtEvent>()
                                for (u in urls) {
                                    try {
                                        val conn = java.net.URL(u).openConnection() as java.net.HttpURLConnection
                                        conn.connectTimeout = 8000
                                        conn.readTimeout = 8000
                                        val text = conn.inputStream.bufferedReader().use { it.readText() }
                                        for (e in IcsClient.parse(text)) {
                                            if (e.id in existingIds) continue
                                            if (e.uid.isNotEmpty() && e.uid in existingUids) continue
                                            added.add(e)
                                        }
                                    } catch (ex: Exception) {
                                        // 单个订阅失败继续
                                    }
                                }
                                withContext(Dispatchers.Main) {
                                    status = "同步完成：新增 ${added.size} 条"
                                    if (added.isNotEmpty()) onImported(added)
                                }
                            }
                        }
                    }) { Text("立即同步", color = Accent) }
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim) }
                }
            }
        }
    }
}

@Composable
private fun BackupManagerDialog(
    repo: CalendarRepository,
    onRestored: (CtStore) -> Unit,
    onDismiss: () -> Unit
) {
    var files by remember { mutableStateOf(repo.backupFiles()) }
    var msg by remember { mutableStateOf("") }
    val ctxB = LocalContext.current
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(14.dp).heightIn(max = 560.dp)) {
                Text("备份管理", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text("自动备份保留最近 10 份；可恢复到任意一份", color = TextDim, fontSize = 10.sp)
                if (msg.isNotEmpty()) {
                    Text(msg, color = Accent, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                }
                var pwd by remember { mutableStateOf("") }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = pwd, onValueChange = { pwd = it },
                        label = { Text("加密口令（加密导出用）") }, singleLine = true,
                        modifier = Modifier.weight(1f)
                    )
                    TextButton(onClick = { pwd = "" }) { Text("清空", color = TextDim, fontSize = 11.sp) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TextButton(onClick = {
                        if (pwd.isBlank()) {
                            msg = "请先填加密口令"
                        } else {
                            try {
                                val ctx2 = ctxB
                                val src = java.io.File(ctx2.filesDir, "calendar.json")
                                if (!src.exists()) {
                                    msg = "没有可导出的数据"
                                } else {
                                    val enc = BackupCrypto.encrypt(src.readText(), pwd)
                                    val f = java.io.File(ctx2.cacheDir, "calendar-backup.enc")
                                    f.writeText(enc)
                                    val uri = androidx.core.content.FileProvider.getUriForFile(
                                        ctx2, "com.caelendar.app.fileprovider", f
                                    )
                                    val share = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
                                        type = "text/plain"
                                        putExtra(android.content.Intent.EXTRA_STREAM, uri)
                                        addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
                                    }
                                    ctx2.startActivity(android.content.Intent.createChooser(share, "导出加密备份"))
                                    msg = "已生成加密备份（AES-GCM）"
                                }
                            } catch (e: Exception) {
                                msg = "加密导出失败：${e.message}"
                            }
                        }
                    }) { Text("加密导出", color = TextDim, fontSize = 11.sp) }
                    TextButton(onClick = {
                        if (pwd.isBlank()) {
                            msg = "请先填加密口令"
                        } else {
                            msg = "用文件选择器打开 .enc 文件（暂用恢复备份入口导入明文）"
                        }
                    }) { Text("加密恢复", color = TextDim, fontSize = 11.sp) }
                    TextButton(onClick = {
                        try {
                            val ctx3 = ctxB
                            val src = java.io.File(ctx3.filesDir, "calendar.json")
                            if (src.exists()) {
                                val dir = java.io.File(ctx3.filesDir, "backup")
                                dir.mkdirs()
                                val stamp = java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US)
                                    .format(java.util.Date())
                                src.copyTo(java.io.File(dir, "snapshot-$stamp.json"), overwrite = true)
                                files = repo.backupFiles()
                                msg = "已创建快照"
                            }
                        } catch (e: Exception) {
                            msg = "快照失败：${e.message}"
                        }
                    }) { Text("立即快照", color = TextDim, fontSize = 11.sp) }
                }
                LazyColumn(Modifier.weight(1f, fill = false).padding(top = 6.dp)) {
                    items(files) { f ->
                        val size = f.length()
                        val events = runCatching {
                            CtStore.fromJsonString(f.readText()).events.size
                        }.getOrDefault(-1)
                        Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(f.name, color = TextMain, fontSize = 11.sp)
                                Text(
                                    "%.1f KB · %s".format(size / 1024.0, if (events >= 0) "$events 条日程" else "解析失败"),
                                    color = TextDim, fontSize = 10.sp
                                )
                            }
                            TextButton(onClick = {
                                val restored = repo.restoreBackup(f)
                                if (restored != null) {
                                    onRestored(restored)
                                } else {
                                    msg = "恢复失败：文件损坏"
                                }
                            }) { Text("恢复", color = Accent, fontSize = 11.sp) }
                            TextButton(onClick = {
                                f.delete()
                                files = repo.backupFiles()
                                msg = "已删除 ${f.name}"
                            }) { Text("删除", color = Color(0xFFFF6B7A), fontSize = 11.sp) }
                        }
                    }
                    if (files.isEmpty()) {
                        item { Text("还没有备份文件", color = TextDim, fontSize = 12.sp) }
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim) }
                }
            }
        }
    }
}

@Composable
private fun SearchDialog(
    store: CtStore,
    onJump: (LocalDate) -> Unit,
    onEdit: (CtEvent) -> Unit,
    onCommand: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var q by remember { mutableStateOf("") }
    val tagById = store.tags.associateBy { it.id }
    val isCommand = q.trimStart().startsWith(">")
    val commands = listOf(
        ">new" to "新建日程",
        ">today" to "回到今天",
        ">month" to "月视图",
        ">week" to "周视图",
        ">day" to "日视图",
        ">year" to "年视图",
        ">list" to "日程列表",
        ">tasks" to "任务看板",
        ">gpa" to "GPA",
        ">settings" to "设置",
        ">date 2026-10-20" to "跳转到指定日期"
    ).filter { it.first.contains(q.trim(), ignoreCase = true) }
    val results = if (q.isBlank() || isCommand) emptyList() else store.events.filter { e ->
        e.title.contains(q, ignoreCase = true) ||
            e.location.contains(q, ignoreCase = true) ||
            e.desc.contains(q, ignoreCase = true) ||
            e.start.contains(q) ||
            e.tags.any { tagById[it]?.name?.contains(q, ignoreCase = true) == true }
    }.sortedBy { it.start }.take(60)

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(14.dp).heightIn(max = 600.dp)) {
                Text("搜索 / 命令", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text("输入关键词搜标题/地点/描述/标签；输入 > 执行命令（如 >today、>week、>date 2026-11-01）", color = TextDim, fontSize = 10.sp)
                OutlinedTextField(
                    value = q, onValueChange = { q = it },
                    label = { Text("搜索或 > 命令") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth().padding(top = 6.dp)
                )
                Spacer(Modifier.height(6.dp))
                LazyColumn(Modifier.weight(1f, fill = false)) {
                    if (isCommand) {
                        items(commands) { (cmd, label) ->
                            Row(
                                Modifier.fillMaxWidth().clickable { onCommand(cmd) }.padding(vertical = 8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(cmd.substringBefore(' '), color = Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                                Spacer(Modifier.width(10.dp))
                                Text(label, color = TextMain, fontSize = 13.sp)
                            }
                        }
                    } else if (q.isNotBlank()) {
                        items(results) { e ->
                            Row(
                                Modifier.fillMaxWidth().clickable {
                                    val d = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull()
                                    if (d != null) onJump(d) else onEdit(e)
                                }.padding(vertical = 8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Box(Modifier.width(4.dp).height(30.dp).background(EvColor(e), RoundedCornerShape(2.dp)))
                                Spacer(Modifier.width(8.dp))
                                Column(Modifier.weight(1f)) {
                                    Text(e.title.ifEmpty { "未命名" }, color = TextMain, fontSize = 14.sp)
                                    Text(
                                        e.start.replace("T", " ") + if (e.location.isNotEmpty()) " · ${e.location}" else "",
                                        color = TextDim, fontSize = 11.sp
                                    )
                                }
                                TextButton(onClick = { onEdit(e) }) { Text("编辑", color = TextDim, fontSize = 11.sp) }
                            }
                        }
                        if (results.isEmpty()) {
                            item { Text("没有匹配的日程", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(vertical = 8.dp)) }
                        }
                    } else {
                        item { Text("输入关键词开始搜索", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(vertical = 8.dp)) }
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim) }
                }
            }
        }
    }
}

@Composable
private fun TabButton(label: String, active: Boolean, onClick: () -> Unit) {
    TextButton(
        onClick = onClick,
        contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 2.dp),
        modifier = Modifier.semantics { contentDescription = "切换到$label" }
    ) {
        Text(
            label,
            color = if (active) Accent else TextDim,
            fontSize = 13.sp,
            maxLines = 1,
            softWrap = false,
            fontWeight = if (active) FontWeight.Bold else FontWeight.Normal
        )
    }
}

private fun RecurFreq(e: CtEvent): Int {
    val r = e.recur()
    return when (r.freq) {
        "daily" -> 1
        "weekly" -> if (r.byDay.sorted() == listOf(1, 2, 3, 4, 5)) 5 else 2
        "monthly" -> 3
        "yearly" -> 4
        else -> 0
    }
}

/** 把事件按重复规则展开到 [from, to]，应用 overrides（桌面 recur.cpp 行为对齐）。 */
fun ExpandEvents(events: List<CtEvent>, from: LocalDate, to: LocalDate): List<CtEvent> {
    val out = mutableListOf<CtEvent>()
    val fmt = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm")
    for (e in events) {
        val rec = e.recur()
        val first = CtEvent.parseDT(e.start) ?: continue
        if (!rec.isRecurring) {
            val d = first.toLocalDate()
            if (!d.isBefore(from) && !d.isAfter(to)) out.add(e)
            continue
        }
        val durMin = java.time.Duration.between(first, CtEvent.parseDT(e.end) ?: first.plusHours(1)).toMinutes()
            .coerceAtLeast(1)
        val starts = rec.startsBetween(first, from.minusDays(31), to)
        val exSet = rec.exDates.toHashSet()
        val ovMap = e.overridesList().associateBy { it.recurId.ifEmpty { it.start } }
        val generated = mutableSetOf<String>()
        for (st in starts) {
            val key = st.format(fmt)
            generated.add(key)
            if (key in exSet) continue
            val ov = ovMap[key]
            if (ov != null) {
                if (ov.deleted) continue
                out.add(ov.copy(id = e.id, instanceDate = key))
            } else {
                out.add(
                    e.copy(
                        start = key,
                        end = st.plusMinutes(durMin).format(fmt),
                        instanceDate = key
                    )
                )
            }
        }
        // 被移动进范围的 override 也要显示
        for (ov in e.overridesList()) {
            if (ov.deleted) continue
            val key = ov.recurId.ifEmpty { ov.start }
            if (key in generated) continue
            val d = runCatching { java.time.LocalDate.parse(ov.start.take(10)) }.getOrNull() ?: continue
            if (d.isBefore(from) || d.isAfter(to)) continue
            out.add(ov.copy(id = e.id, instanceDate = key))
        }
    }
    return out.sortedWith(
        compareBy<CtEvent> { if (it.allDay) 0 else 1 }.thenBy { it.start }
    )
}

private var gTags: List<CtTag> = emptyList()

private fun EvColor(e: CtEvent): Color {
    return try {
        if (e.color.length == 7 && e.color.startsWith("#")) {
            Color(android.graphics.Color.parseColor(e.color))
        } else {
            val tagColor = e.tags.firstOrNull()?.let { tid -> gTags.firstOrNull { it.id == tid }?.color }
            if (tagColor != null && tagColor.length == 7 && tagColor.startsWith("#")) {
                Color(android.graphics.Color.parseColor(tagColor))
            } else {
                Accent
            }
        }
    } catch (_: Exception) {
        Accent
    }
}

private fun DaysOfWeek(day: LocalDate): List<LocalDate> {
    val monday = day.minusDays((day.dayOfWeek.value - 1).toLong())
    return (0..6).map { monday.plusDays(it.toLong()) }
}

private val WeekNames = listOf("周一", "周二", "周三", "周四", "周五", "周六", "周日")

@Composable
private fun ViewModeBar(mode: Int, onMode: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        listOf("月", "周", "日", "年").forEachIndexed { i, label ->
            val active = mode == i
            Box(
                Modifier
                    .background(if (active) Accent.copy(alpha = 0.22f) else CardColor, RoundedCornerShape(8.dp))
                    .clickable { onMode(i) }
                    .padding(horizontal = 16.dp, vertical = 6.dp)
            ) {
                Text(
                    label,
                    color = if (active) Accent else TextDim,
                    fontSize = 13.sp,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Normal
                )
            }
        }
    }
}

@Composable
private fun TimelineScreen(
    store: CtStore,
    days: List<LocalDate>,
    selectedDay: LocalDate,
    onSelectDay: (LocalDate) -> Unit,
    onEdit: (CtEvent) -> Unit,
    onNew: (LocalDate, Int) -> Unit,
    onReschedule: ((CtEvent, Int) -> Unit)? = null,
    onMoveDate: ((CtEvent, Int) -> Unit)? = null
) {
    val hourHeight = 36
    val density = androidx.compose.ui.platform.LocalDensity.current
    val hourHeightPx = with(density) { hourHeight.dp.toPx() }
    // 先按重复规则展开，再过滤本周/当日（修复重复系列在周/日视图不显示）
    val expandedAll = ExpandEvents(store.events, days.first(), days.last())
    val events = expandedAll.filter { e ->
        if (e.start.length < 16 || days.none { e.start.startsWith(it.toString()) }) return@filter false
        // 跨日事件（结束日期晚于开始日期）只放到顶部跨日条，不在时间轴里重复画
        val sd = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull()
        val ed = if (e.end.length >= 16) runCatching { LocalDate.parse(e.end.take(10)) }.getOrNull() else null
        !(sd != null && ed != null && ed.isAfter(sd))
    }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        // 日期头
        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp, vertical = 4.dp)) {
            Spacer(Modifier.width(42.dp))
            for (d in days) {
                Box(
                    Modifier.weight(1f).clickable { onSelectDay(d) },
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            if (days.size == 1) "周${WeekNames[d.dayOfWeek.value - 1].substring(1)}" else WeekNames[d.dayOfWeek.value - 1],
                            color = TextDim, fontSize = 10.sp
                        )
                        Text(
                            "${d.dayOfMonth}",
                            color = if (d == selectedDay) Color(0xFF141824) else TextMain,
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier
                                .background(if (d == selectedDay) Accent else Color.Transparent, RoundedCornerShape(50))
                                .padding(horizontal = 8.dp, vertical = 2.dp)
                        )
                    }
                }
            }
        }
        // 跨日事件条（周/日视图顶部）：按天分段——起始日实线边框，后续日虚线边框
        if (true) {
            val crossEvents = store.events.filter { e ->
                e.start.length >= 16 && e.end.length >= 16 &&
                runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull()?.let { sd ->
                    runCatching { LocalDate.parse(e.end.take(10)) }.getOrNull()?.let { ed ->
                        val ws = days.first()
                        ed.isAfter(sd) && !ed.isBefore(ws) && !sd.isAfter(days.last())
                    } ?: false
                } ?: false
            }
            if (crossEvents.isNotEmpty()) {
                val rowH = 20.dp
                BoxWithConstraints(Modifier.fillMaxWidth().height(rowH * crossEvents.size)) {
                    val timeCol2 = 42.dp
                    val colW2 = (maxWidth - timeCol2) / days.size
                    for ((ci, e) in crossEvents.withIndex()) {
                        val sd = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull() ?: continue
                        val ed = runCatching { LocalDate.parse(e.end.take(10)) }.getOrNull() ?: continue
                        val borderColor = EvColor(e).copy(alpha = 0.95f)
                        for ((di, d) in days.withIndex()) {
                            if (d.isBefore(sd) || d.isAfter(ed)) continue
                            val isStart = d == sd
                            val label = when {
                                isStart && d == ed -> e.title
                                isStart -> e.title.ifEmpty { "跨日" } + " →"
                                d == ed -> "← " + e.title.ifEmpty { "跨日" }
                                else -> "← ⋯ →"
                            }
                            Box(
                                Modifier
                                    .offset(x = timeCol2 + colW2 * di + 1.dp, y = (ci * 20).dp)
                                    .width(colW2 - 2.dp)
                                    .height(16.dp)
                                    .background(
                                        EvColor(e).copy(alpha = if (isStart) 0.35f else 0.18f),
                                        RoundedCornerShape(3.dp)
                                    )
                                    .then(
                                        if (isStart) {
                                            // 起始日：实线
                                            Modifier.border(1.5.dp, borderColor, RoundedCornerShape(3.dp))
                                        } else {
                                            // 后续日：虚线
                                            Modifier.drawBehind {
                                                drawRoundRect(
                                                    color = borderColor,
                                                    cornerRadius = androidx.compose.ui.geometry.CornerRadius(3.dp.toPx()),
                                                    style = androidx.compose.ui.graphics.drawscope.Stroke(
                                                        width = 1.2.dp.toPx(),
                                                        pathEffect = androidx.compose.ui.graphics.PathEffect.dashPathEffect(
                                                            floatArrayOf(5.dp.toPx(), 3.dp.toPx()), 0f
                                                        )
                                                    )
                                                )
                                            }
                                        }
                                    )
                                    .clickable { onEdit(e) }
                                    .padding(horizontal = 2.dp),
                                contentAlignment = Alignment.CenterStart
                            ) {
                                Text(
                                    label, color = TextMain, fontSize = 8.sp,
                                    maxLines = 1, overflow = TextOverflow.Ellipsis
                                )
                            }
                        }
                    }
                }
            }
        }
        BoxWithConstraints(Modifier.fillMaxWidth().height((hourHeight * 24).dp)) {
            val timeCol = 42.dp
            val colW = (maxWidth - timeCol) / days.size
            // 背景：小时行 + 点击新建
            Column(Modifier.fillMaxSize()) {
                for (h in 0..23) {
                    Row(Modifier.fillMaxWidth().height(hourHeight.dp)) {
                        Text(
                            "%02d:00".format(h),
                            color = WithDim(),
                            fontSize = 9.sp,
                            modifier = Modifier.width(timeCol).padding(start = 2.dp)
                        )
                        for (d in days) {
                            Box(
                                Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                                    .background(if (h % 2 == 0) WithAlpha(CardColor, 0.5f) else Color.Transparent)
                                    .clickable { onSelectDay(d); onNew(d, h) }
                            )
                        }
                    }
                    if (h < 23) {
                        Box(Modifier.fillMaxWidth().height(0.5.dp).background(WithAlpha(TextDim, 0.15f)))
                    }
                }
            }
            // 当前时间线（今天在视图范围内时显示）
            val nowDayIdx = days.indexOfFirst { it == LocalDate.now() }
            if (nowDayIdx >= 0) {
                val now = java.time.LocalTime.now()
                val nowY = ((now.hour * 60 + now.minute) / 60f * hourHeight).dp
                Box(
                    Modifier
                        .offset(x = timeCol + colW * nowDayIdx, y = nowY)
                        .width(colW)
                        .height(1.5.dp)
                        .background(Color(0xFFFF5252))
                )
            }
            // 事件块（长按拖动改时间，15 分钟吸附）
            var dragEventId by remember { mutableStateOf("") }
            var dragDeltaY by remember { mutableStateOf(0f) }
            var dragDeltaX by remember { mutableStateOf(0f) }
            val colWidthPx = with(androidx.compose.ui.platform.LocalDensity.current) { colW.toPx() }
            val eventsByDay = days.map { d -> events.filter { it.start.startsWith(d.toString()) }.sortedBy { it.start } }
            for (e in events) {
                val idx = days.indexOfFirst { e.start.startsWith(it.toString()) }
                if (idx < 0) continue
                val sameDay = eventsByDay[idx]
                val pos = sameDay.indexOfFirst { it.id == e.id && it.start == e.start && it.title == e.title }
                val prevOverlap = pos > 0 && sameDay[pos - 1].end.take(16) > e.start.take(16)
                val nextOverlap = pos >= 0 && pos + 1 < sameDay.size && sameDay[pos + 1].start.take(16) < e.end.take(16)
                val stacked = prevOverlap || nextOverlap
                val sh = e.start.substring(11, 13).toIntOrNull() ?: 0
                val sm = e.start.substring(14, 16).toIntOrNull() ?: 0
                val eh = e.end.substring(11, 13).toIntOrNull() ?: (sh + 1)
                val em = e.end.substring(14, 16).toIntOrNull() ?: 0
                var startMin = sh * 60 + sm
                var endMin = eh * 60 + em
                if (endMin <= startMin) endMin = startMin + 30
                startMin = startMin.coerceIn(0, 24 * 60 - 15)
                endMin = endMin.coerceIn(startMin + 15, 24 * 60)
                val yTop = (startMin / 60f * hourHeight).dp
                val yH = ((endMin - startMin) / 60f * hourHeight).dp
                val laneW = if (stacked) (colW - 4.dp) / 2 else colW - 4.dp
                val laneX = timeCol + colW * idx + 2.dp + (if (stacked && prevOverlap) laneW else 0.dp)
                Box(
                    Modifier
                        .offset(x = laneX, y = yTop)
                        .width(laneW)
                        .height(yH.coerceAtLeast(20.dp))
                        .then(
                            if (onReschedule != null) Modifier.pointerInput(e.id, e.start) {
                                // 长按 2 秒（原位不动）后才进入拖动；期间抬手 = 打开编辑
                                awaitEachGesture {
                                    val down = awaitFirstDown()
                                    var lastPos = down.position
                                    var deltaY = 0f
                                    var deltaX = 0f
                                    val longPressStarted = withTimeoutOrNull(2000L) {
                                        while (true) {
                                            val event = awaitPointerEvent()
                                            val change = event.changes.firstOrNull { it.id == down.id }
                                                ?: return@withTimeoutOrNull false
                                            if (!change.pressed) {
                                                onEdit(e)
                                                return@withTimeoutOrNull false
                                            }
                                            if (change.positionChange().getDistance() > viewConfiguration.touchSlop) {
                                                return@withTimeoutOrNull false
                                            }
                                        }
                                        false
                                    }
                                    if (longPressStarted == null) {
                                        down.consume()
                                        dragEventId = e.id
                                        while (true) {
                                            val event = awaitPointerEvent()
                                            val change = event.changes.firstOrNull { it.id == down.id } ?: break
                                            deltaY += change.position.y - lastPos.y
                                            deltaX += change.position.x - lastPos.x
                                            lastPos = change.position
                                            change.consume()
                                            if (!change.pressed) break
                                        }
                                        val minutes = if (hourHeightPx > 0f) {
                                            (deltaY / hourHeightPx * 60f / 15f).toInt() * 15
                                        } else 0
                                        if (minutes != 0) onReschedule?.invoke(e, minutes)
                                        val colDelta = if (colWidthPx > 0f && onMoveDate != null) {
                                            Math.round(deltaX / colWidthPx)
                                        } else 0
                                        if (colDelta != 0) onMoveDate?.invoke(e, colDelta)
                                        dragEventId = ""
                                    }
                                }
                            } else Modifier
                        )
                        .background(EvColor(e).copy(alpha = 0.4f), RoundedCornerShape(4.dp))
                        .padding(horizontal = 1.5.dp, vertical = 1.dp)
                ) {
                    Text(
                        e.title.ifEmpty { "未命名" },
                        color = TextMain,
                        // 用 dp 换算成 sp：不跟随系统字体缩放，行距按块高压紧
                        fontSize = with(density) { 9.dp.toSp() },
                        lineHeight = with(density) { 10.5.dp.toSp() },
                        letterSpacing = 0.sp,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis
                    )
                }
            }
        }
        Spacer(Modifier.height(12.dp))
        Text("点击时间格新建 · 点击日程编辑 · 长按 2 秒后拖动改时间（15 分钟吸附）", color = TextDim, fontSize = 10.sp, modifier = Modifier.padding(horizontal = 12.dp))
        Spacer(Modifier.height(24.dp))
    }
}

private fun WithDim(): Color = TextDim
private fun WithAlpha(c: Color, a: Float): Color = c.copy(alpha = a)

@Composable
private fun YearView(store: CtStore, year: Int, onPickMonth: (Int) -> Unit) {
    // 有事件的日期集合（展开重复规则后），避免每格遍历全部事件
    val daysWithEvents = remember(store.events, year) {
        val set = mutableSetOf<String>()
        runCatching {
            for (e in ExpandEvents(store.events, LocalDate.of(year, 1, 1), LocalDate.of(year, 12, 31))) {
                if (e.start.length >= 10) set.add(e.start.take(10))
            }
        }
        set
    }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("$year 年", color = TextMain, fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.weight(1f))
            Text("用上方 ‹ › 切换年", color = TextDim, fontSize = 10.sp)
        }
        Text("按事件密度着色，点击月份进入月视图", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(vertical = 6.dp))
        for (row in 0..3) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                for (col in 0..2) {
                    val m = row * 3 + col + 1
                    val prefix = "$year-${"%02d".format(m)}"
                    val count = store.events.count { it.start.startsWith(prefix) }
                    val heat = when {
                        count >= 6 -> 0.45f
                        count >= 3 -> 0.28f
                        count >= 1 -> 0.14f
                        else -> 0f
                    }
                    Card(
                        Modifier.weight(1f).clickable { onPickMonth(m) },
                        colors = CardDefaults.cardColors(
                            containerColor = if (ThemeState.dark) Color(0xFF10131C) else CardColor
                        )
                    ) {
                        Column(Modifier.padding(10.dp)) {
                            Text("$m 月", color = TextMain, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                            Text("$count 项", color = if (count > 0) Accent else TextDim, fontSize = 11.sp)
                            Spacer(Modifier.height(4.dp))
                            // 迷你月历：7 列点阵，有事件的日子画点
                            val firstDay = runCatching { java.time.LocalDate.of(year, m, 1) }.getOrNull()
                            if (firstDay != null) {
                                val startOffset = (firstDay.dayOfWeek.value + 6) % 7
                                val dim = firstDay.lengthOfMonth()
                                val cells2 = startOffset + dim
                                for (rw in 0 until ((cells2 + 6) / 7)) {
                                    Row(Modifier.padding(vertical = 0.5.dp)) {
                                        for (cd in 0 until 7) {
                                            val idx2 = rw * 7 + cd
                                            val dayNum = idx2 - startOffset + 1
                                            val has = dayNum in 1..dim &&
                                                "$year-${"%02d".format(m)}-${"%02d".format(dayNum)}" in daysWithEvents
                                            Box(
                                                Modifier.size(3.5.dp).padding(0.3.dp)
                                                    .background(
                                                        if (has) Accent.copy(alpha = 0.75f) else TextDim.copy(alpha = 0.10f),
                                                        RoundedCornerShape(50)
                                                    )
                                            )
                                        }
                                    }
                                }
                            }
                            Spacer(Modifier.height(4.dp))
                            Box(
                                Modifier.fillMaxWidth().height(4.dp)
                                    .background(Accent.copy(alpha = heat.coerceAtLeast(0.07f)), RoundedCornerShape(3.dp))
                            )
                        }
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
        }
        Spacer(Modifier.height(24.dp))
    }
}

@Composable
private fun BoardColumn(
    title: String,
    list: List<CtEvent>,
    onDone: ((CtEvent) -> Unit)?,
    modifier: Modifier
) {
    Column(modifier) {
        Text(title, color = TextDim, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(6.dp))
        LazyColumn(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            items(list) { e ->
                Card(
                    Modifier.fillMaxWidth().clickable { onDone?.invoke(e) },
                    colors = CardDefaults.cardColors(containerColor = CardColor)
                ) {
                    Column(Modifier.padding(10.dp)) {
                        Text(
                            e.title.ifEmpty { "未命名" },
                            color = TextMain, fontSize = 13.sp, fontWeight = FontWeight.Medium,
                            maxLines = 2, overflow = TextOverflow.Ellipsis
                        )
                        Text(
                            if (e.end.length >= 16) "截止 ${e.end.substring(5, 16).replace('T', ' ')}" else "截止 ${e.end}",
                            color = TextDim, fontSize = 10.sp
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun MonthScreen(
    store: CtStore,
    month: YearMonth,
    selectedDay: LocalDate,
    onMonthChange: (YearMonth) -> Unit,
    onSelectDay: (LocalDate) -> Unit,
    onEdit: (CtEvent) -> Unit,
    onCreate: () -> Unit,
    onTemplates: () -> Unit = {},
    onBulk: () -> Unit = {},
    water: Map<String, Int> = emptyMap(),
    onWater: (String, Int) -> Unit = { _, _ -> },
    onLongPressEvent: (CtEvent) -> Unit = {},
    onLongPressDay: (LocalDate) -> Unit = {},
    onDayDetail: (LocalDate) -> Unit = {},
    hasClipboard: Boolean = false,
    onMulti: () -> Unit = {},
    batchMode: Boolean = false,
    batchSelected: List<String> = emptyList(),
    onEnterBatch: (CtEvent) -> Unit = {},
    onToggleBatch: (CtEvent) -> Unit = {},
    onExitBatch: () -> Unit = {},
    onBatchAction: (String) -> Unit = {}
) {
    val first = month.atDay(1)
    val monthDensity = androidx.compose.ui.platform.LocalDensity.current
    // 整月只展开一次，格子里只做过滤（原来每格 ExpandEvents 一次，主线程会卡 2 秒+）
    val monthExpanded = remember(store.events, month) {
        ExpandEvents(store.events, first, month.atEndOfMonth())
    }
    // 跨日事件（非全天，结束日期晚于开始日期）——桌面式按天分割显示
    val crossEvents = monthExpanded.filter { e ->
        if (e.allDay || e.start.length < 16 || e.end.length < 16) return@filter false
        val sd = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull() ?: return@filter false
        val ed = runCatching { LocalDate.parse(e.end.take(10)) }.getOrNull() ?: return@filter false
        ed.isAfter(sd)
    }
    val startOffset = (first.dayOfWeek.value - DayOfWeek.MONDAY.value + 7) % 7
    val days = month.lengthOfMonth()
    val cells = (0 until startOffset).map { null } + (1..days).map { month.atDay(it) }

    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(12.dp)) {
        val todayKey = LocalDate.now().toString()
        val cups = water[todayKey] ?: 0
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text("💧 喝水", color = TextDim, fontSize = 12.sp)
            Spacer(Modifier.width(6.dp))
            for (i in 1..8) {
                Box(
                    Modifier.size(18.dp).padding(1.dp)
                        .background(
                            if (i <= cups) Color(0xFF4FC3F7) else CardColor,
                            RoundedCornerShape(50)
                        )
                        .clickable { onWater(todayKey, if (cups == i) i - 1 else i) }
                )
            }
            Spacer(Modifier.width(6.dp))
            Text("$cups/8", color = if (cups > 0) Accent else TextDim, fontSize = 12.sp)
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            TextButton(
                onClick = { onMonthChange(month.minusMonths(1)) },
                modifier = Modifier.semantics { contentDescription = "上一个月" }
            ) { Text("‹", color = Accent, fontSize = 22.sp) }
            Text("${month.year} 年 ${month.monthValue} 月", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
            TextButton(onClick = { onMonthChange(month.plusMonths(1)) }) { Text("›", color = Accent, fontSize = 22.sp) }
        }
        Row(Modifier.fillMaxWidth()) {
            for (wd in listOf("一", "二", "三", "四", "五", "六", "日")) {
                Text(wd, color = TextDim, fontSize = 12.sp, modifier = Modifier.weight(1f))
            }
        }
        for (week in cells.chunked(7)) {
            BoxWithConstraints(Modifier.fillMaxWidth()) {
            val rowColW = maxWidth / 7
            Row(Modifier.fillMaxWidth()) {
                for (day in week) {
                    Box(
                        Modifier.weight(1f).height(74.dp).padding(2.dp)
                            .background(
                                when {
                                    day == selectedDay && ThemeState.dark -> Color(0xFF2A2416)
                                    day == selectedDay -> Accent.copy(alpha = 0.25f)
                                    ThemeState.dark -> Color(0xFF10131C)
                                    else -> CardColor
                                },
                                RoundedCornerShape(8.dp)
                            )
                            .pointerInput(day) {
                                detectTapGestures(
                                    onTap = { if (day != null) onSelectDay(day) },
                                    onLongPress = {
                                        if (day != null) {
                                            if (hasClipboard) onLongPressDay(day) else onDayDetail(day)
                                        }
                                    }
                                )
                            },
                        contentAlignment = Alignment.TopStart
                    ) {
                        if (day != null) {
                            val dayEvents = monthExpanded
                                .filter { it.start.startsWith(day.toString()) }
                                .sortedBy { it.start }
                            Column(Modifier.padding(3.dp)) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(day.dayOfMonth.toString(), color = if (day == LocalDate.now()) Accent else TextMain, fontSize = 11.sp)
                                    Spacer(Modifier.width(2.dp))
                                    val lunarFest = LunarCalendar.festival(day)
                                    val solarFest = Holiday.solar(day)
                                    val term = SolarTerms.nameOn(day)
                                    val lunarLabel = lunarFest ?: solarFest ?: term ?: LunarCalendar.dayText(day)
                                    Text(
                                        lunarLabel,
                                        color = if (lunarFest != null || solarFest != null || term != null) Accent else TextDim,
                                        fontSize = 7.sp,
                                        maxLines = 1
                                    )
                                }
                                if (dayEvents.isNotEmpty()) {
                                       Text("${dayEvents.size} 项", color = Accent, fontSize = 7.sp, fontWeight = FontWeight.Bold)
                                   }
                                   for (e in dayEvents.take(3)) {
                                    Box(
                                        Modifier.fillMaxWidth().padding(top = 1.dp).height(16.dp)
                                            .background(EvColor(e).copy(alpha = 0.32f), RoundedCornerShape(3.dp))
                                            .then(
                                                if (batchMode && "${e.id}@${e.start}" in batchSelected) {
                                                    Modifier.border(1.5.dp, Accent, RoundedCornerShape(3.dp))
                                                } else Modifier
                                            )
                                            .pointerInput(e.id, e.start, batchMode) {
                                                detectTapGestures(
                                                    onTap = {
                                                        if (batchMode) onToggleBatch(e) else onEdit(e)
                                                    },
                                                    onLongPress = {
                                                        if (batchMode) onToggleBatch(e) else onEnterBatch(e)
                                                    }
                                                )
                                            }
                                            .padding(horizontal = 2.dp),
                                        contentAlignment = Alignment.CenterStart
                                    ) {
                                        Text(
                                            (if (e.start.length >= 16) e.start.substring(11, 16) + " " else "") +
                                                e.title.ifEmpty { "未命名" },
                                            color = TextMain,
                                            fontSize = with(monthDensity) { 7.dp.toSp() },
                                            lineHeight = with(monthDensity) { 8.dp.toSp() },
                                            letterSpacing = 0.sp,
                                            maxLines = 1, overflow = TextOverflow.Ellipsis
                                        )
                                    }
                                }
                                if (dayEvents.size > 3) {
                                    Text("+${dayEvents.size - 3}", color = Accent, fontSize = 7.sp)
                                }
                                // 跨日事件：每天一段（首日 →、末日 ←、中间 ⋯）
                                var crossShown = 0
                                for (ce in crossEvents) {
                                    if (crossShown >= 1) break
                                    val sd = runCatching { LocalDate.parse(ce.start.take(10)) }.getOrNull() ?: continue
                                    val ed = runCatching { LocalDate.parse(ce.end.take(10)) }.getOrNull() ?: continue
                                    if (day.isBefore(sd) || day.isAfter(ed)) continue
                                    val label = when {
                                        day == sd && day == ed -> ce.title.ifEmpty { "跨日" }
                                        day == sd -> ce.title.ifEmpty { "跨日" } + " →"
                                        day == ed -> "← " + ce.title.ifEmpty { "跨日" }
                                        else -> "← ⋯ →"
                                    }
                                    Box(
                                        Modifier.fillMaxWidth().padding(top = 1.dp).height(14.dp)
                                            .background(EvColor(ce).copy(alpha = 0.45f), RoundedCornerShape(3.dp))
                                            .clickable { onEdit(ce) }
                                            .padding(horizontal = 2.dp),
                                        contentAlignment = Alignment.CenterStart
                                    ) {
                                        Text(
                                            label, color = TextMain,
                                            fontSize = with(monthDensity) { 7.dp.toSp() },
                                            lineHeight = with(monthDensity) { 8.dp.toSp() },
                                            letterSpacing = 0.sp,
                                            maxLines = 1, overflow = TextOverflow.Ellipsis
                                        )
                                    }
                                    crossShown++
                                }
                            }
                        }
                    }
                }
            }

            }
        }
        Spacer(Modifier.height(10.dp))
        Row(
            Modifier.fillMaxWidth().padding(top = 2.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                "${selectedDay} 日程",
                color = TextMain, fontWeight = FontWeight.Bold, fontSize = 15.sp,
                maxLines = 1, softWrap = false,
                modifier = Modifier
                    .weight(1f, fill = false)
                    .clickable { onDayDetail(selectedDay) }
            )
            Spacer(Modifier.width(6.dp))
            TextButton(
                onClick = onBulk,
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
            ) { Text("批量", color = TextDim, fontSize = 12.sp, maxLines = 1, softWrap = false) }
            if (batchMode) {
                Text("已选 ${batchSelected.size}", color = Accent, fontSize = 12.sp, maxLines = 1)
                TextButton(
                    onClick = { onBatchAction("done") },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
                ) { Text("完成", color = TextDim, fontSize = 12.sp, maxLines = 1) }
                TextButton(
                    onClick = { onBatchAction("delete") },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
                ) { Text("删除", color = Color(0xFFFF6B7A), fontSize = 12.sp, maxLines = 1) }
                TextButton(
                    onClick = { onBatchAction("tag") },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
                ) { Text("加标签", color = TextDim, fontSize = 12.sp, maxLines = 1) }
                TextButton(
                    onClick = onExitBatch,
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
                ) { Text("取消", color = TextDim, fontSize = 12.sp, maxLines = 1) }
            } else {
                TextButton(
                    onClick = onMulti,
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                    modifier = Modifier.semantics { contentDescription = "多选日程批量操作" }
                ) { Text("多选", color = TextDim, fontSize = 12.sp, maxLines = 1, softWrap = false) }
                TextButton(
                    onClick = onTemplates,
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)
                ) { Text("模板", color = TextDim, fontSize = 12.sp, maxLines = 1, softWrap = false) }
                Spacer(Modifier.width(4.dp))
                Button(
                    onClick = onCreate,
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 14.dp, vertical = 0.dp),
                    modifier = Modifier.height(34.dp).semantics { contentDescription = "新建日程" }
                ) {
                    Text("+ 新建", fontSize = 13.sp, maxLines = 1, softWrap = false)
                }
            }
        }
        val dayEvents = ExpandEvents(store.events, selectedDay, selectedDay)
            .filter { it.start.startsWith(selectedDay.toString()) }.sortedBy { it.start }
        if (dayEvents.isEmpty()) {
            Text("这一天还没有日程", color = TextDim, modifier = Modifier.padding(vertical = 12.dp))
        }
        for (e in dayEvents) {
            // 长按 → 进入多选（与事件条一致）；编辑/删除等操作在多选批量栏里
            EventRow(e, onEdit, onLongPress = { onEnterBatch(it) })
        }
    }
}

@Composable
private fun ListScreen(store: CtStore, onEdit: (CtEvent) -> Unit) {
    var query by remember { mutableStateOf("") }
    var filter by remember { mutableStateOf(0) } // 0 全部 1 今天 2 未来7天 3 未完成 4 已完成
    val today = LocalDate.now()
    val todayStr = today.toString()
    val weekEnd = today.plusDays(7).toString()
    val expanded = remember(store.events) { ExpandEvents(store.events, today.minusDays(1), today.plusWeeks(8)) }
    val base = expanded.filter { e ->
        query.isEmpty() || e.title.contains(query, ignoreCase = true) || e.location.contains(query, ignoreCase = true)
    }
    val filtered = when (filter) {
        1 -> base.filter { it.start.startsWith(todayStr) }
        2 -> base.filter { it.start >= todayStr && it.start < weekEnd }
        3 -> base.filter { !it.done }
        4 -> base.filter { it.done }
        else -> base
    }.sortedBy { it.start }
    Column(Modifier.fillMaxSize().padding(12.dp)) {
        Text("日程列表", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        OutlinedTextField(
            value = query, onValueChange = { query = it },
            label = { Text("搜索标题 / 地点") },
            modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)
        )
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf("全部", "今天", "未来7天", "未完成", "已完成").forEachIndexed { i, label ->
                Box(
                    Modifier
                        .background(if (filter == i) Accent.copy(alpha = 0.25f) else CardColor, RoundedCornerShape(8.dp))
                        .clickable { filter = i }
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                ) {
                    Text(label, color = if (filter == i) Accent else TextDim, fontSize = 12.sp)
                }
            }
        }
        Spacer(Modifier.height(8.dp))
        LazyColumn {
            items(filtered, key = { it.id + "@" + it.start }) { e ->
            EventRow(e, onEdit)
        }
            if (filtered.isEmpty()) item { Text("没有符合条件的日程", color = TextDim, modifier = Modifier.padding(top = 8.dp)) }
        }
    }
}

@Composable
private fun TagManagerDialog(store: CtStore, onStore: (CtStore) -> Unit, onDismiss: () -> Unit) {
    var name by remember { mutableStateOf("") }
    var colorIdx by remember { mutableStateOf(0) }
    val palette = listOf("#6EA8FF", "#FFB454", "#FF6B7A", "#A3E635", "#C084FC", "#F472B6", "#22D3EE")
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 480.dp)) {
                Text("标签管理", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState())) {
                    store.tags.forEach { tag ->
                        Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                            val tc = try {
                                if (tag.color.length == 7 && tag.color.startsWith("#")) Color(android.graphics.Color.parseColor(tag.color)) else Accent
                            } catch (_: Exception) {
                                Accent
                            }
                            Box(Modifier.size(12.dp).background(tc, RoundedCornerShape(50)))
                            Spacer(Modifier.width(6.dp))
                            val iconChoices = listOf("📚", "📝", "🎯", "🏠", "🧪", "💡", "🎵", "⚽", "☕", "⭐")
                            val iconMap = mapOf(
                                "book" to "📚", "pen" to "📝", "edit" to "✏️", "target" to "🎯",
                                "home" to "🏠", "flask" to "🧪", "users" to "👥", "star" to "⭐",
                                "course" to "📚", "study" to "📝", "exam" to "🎯", "life" to "🏠"
                            )
                            Text(
                                iconMap[tag.icon] ?: tag.icon.ifEmpty { "📚" },
                                fontSize = 14.sp,
                                modifier = Modifier.clickable {
                                    val cur = iconChoices.indexOf(iconMap[tag.icon] ?: tag.icon.ifEmpty { "📚" })
                                    val next = iconChoices[(if (cur < 0) 0 else (cur + 1) % iconChoices.size)]
                                    val newTags = store.tags.map {
                                        if (it.id == tag.id) it.copy(icon = next) else it
                                    }.toMutableList()
                                    onStore(store.copy(tags = newTags))
                                }
                            )
                            Spacer(Modifier.width(6.dp))
                            Text(tag.name, color = TextMain, fontSize = 13.sp, modifier = Modifier.weight(1f))
                            Text(
                                "${store.events.count { e -> e.tags.contains(tag.id) }} 项",
                                color = TextDim, fontSize = 10.sp
                            )
                            TextButton(onClick = {
                                val newTags = store.tags.filter { it.id != tag.id }.toMutableList()
                                onStore(store.copy(tags = newTags))
                            }) { Text("删除", color = TextDim, fontSize = 11.sp) }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = name, onValueChange = { name = it },
                        label = { Text("新标签名") },
                        modifier = Modifier.weight(1f)
                    )
                    Spacer(Modifier.width(6.dp))
                    Button(onClick = {
                        if (name.isNotBlank()) {
                            val newTags = (store.tags + CtTag(
                                id = "custom-" + UUID.randomUUID().toString().take(8),
                                name = name.trim(),
                                color = palette[colorIdx]
                            )).toMutableList()
                            onStore(store.copy(tags = newTags))
                            name = ""
                        }
                    }) { Text("添加") }
                }
                Row(Modifier.padding(top = 6.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    palette.forEachIndexed { i, hex ->
                        val c = Color(android.graphics.Color.parseColor(hex))
                        Box(
                            Modifier.size(24.dp).background(c, RoundedCornerShape(50))
                                .then(if (colorIdx == i) Modifier.border(2.dp, TextMain, RoundedCornerShape(50)) else Modifier)
                                .clickable { colorIdx = i }
                        )
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun GpaScreen(repo: GpaRepository) {
    val ctx = LocalContext.current
    var list by remember { mutableStateOf<List<GradeRec>>(repo.load()) }
    var course by remember { mutableStateOf("") }
    var credit by remember { mutableStateOf("") }
    var score by remember { mutableStateOf("") }
    var editingIndex by remember { mutableStateOf(-1) }
    val importLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            try {
                val text = ctx.contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() } ?: ""
                val arr = if (text.trimStart().startsWith("[")) JSONArray(text) else JSONArray()
                val imported = mutableListOf<GradeRec>()
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    val c = o.optString("course")
                    val cr = o.optDouble("credit", 0.0)
                    val sc = o.optDouble("score", 0.0)
                    if (c.isNotEmpty() && cr > 0) imported.add(GradeRec(c, cr, sc))
                }
                if (imported.isNotEmpty()) {
                    list = imported
                    repo.save(list)
                }
            } catch (_: Exception) {
            }
        }
    }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(12.dp)) {
        Text("成绩 / GPA", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        Text(
            "GPA %.2f · 总学分 %.1f · %d 门课".format(
                GpaRepository.weightedGpa(list), GpaRepository.totalCredits(list), list.size
            ),
            color = Accent, fontSize = 13.sp, modifier = Modifier.padding(vertical = 6.dp)
        )
        Text("4.0 制：90+=4.0 · 85=3.7 · 82=3.3 · 78=3.0 · 75=2.7 · 72=2.3 · 68=2.0 · 64=1.5 · 60=1.0",
            color = TextDim, fontSize = 10.sp, modifier = Modifier.padding(bottom = 8.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(value = course, onValueChange = { course = it }, label = { Text("课程名") }, modifier = Modifier.weight(1.6f))
            Spacer(Modifier.width(6.dp))
            OutlinedTextField(value = credit, onValueChange = { credit = it }, label = { Text("学分") }, modifier = Modifier.weight(0.8f))
            Spacer(Modifier.width(6.dp))
            OutlinedTextField(value = score, onValueChange = { score = it }, label = { Text("成绩") }, modifier = Modifier.weight(0.8f))
        }
        Row(Modifier.padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                val c = credit.toDoubleOrNull()
                val sc = score.toDoubleOrNull()
                if (course.isNotBlank() && c != null && sc != null && c > 0 && sc >= 0 && sc <= 100) {
                    list = list + GradeRec(course.trim(), c, sc)
                    repo.save(list)
                    course = ""
                    credit = ""
                    score = ""
                }
            }) { Text("添加") }
            TextButton(onClick = {
                try {
                    val arr = JSONArray()
                    for (g in list) {
                        arr.put(JSONObject().put("course", g.course).put("credit", g.credit).put("score", g.score))
                    }
                    val f = File(ctx.cacheDir, "grades.json")
                    f.writeText(arr.toString())
                    val uri = FileProvider.getUriForFile(ctx, "com.caelendar.app.fileprovider", f)
                    val share = Intent(Intent.ACTION_SEND).apply {
                        type = "application/json"
                        putExtra(Intent.EXTRA_STREAM, uri)
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                    ctx.startActivity(Intent.createChooser(share, "导出成绩"))
                } catch (_: Exception) {
                }
            }) { Text("导出", color = TextDim) }
            TextButton(onClick = { importLauncher.launch(arrayOf("application/json", "text/*", "*/*")) }) {
                Text("导入", color = TextDim)
            }
        }
        val grouped = list.withIndex().groupBy { it.value.term.ifEmpty { "未分组" } }
        for ((term, items) in grouped) {
            val sub = items.map { it.value }
            Text(
                "$term · GPA %.2f · %.1f 学分".format(GpaRepository.weightedGpa(sub), GpaRepository.totalCredits(sub)),
                color = Accent, fontSize = 12.sp, fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(top = 8.dp, bottom = 2.dp)
            )
            for ((idx, g) in items) {
                Card(
                    Modifier.fillMaxWidth().padding(vertical = 4.dp),
                    colors = CardDefaults.cardColors(containerColor = CardColor)
                ) {
                    Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(g.course, color = TextMain, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                            Text("学分 %.1f · 成绩 %.0f · 绩点 %.2f".format(g.credit, g.score, g.gpa()), color = TextDim, fontSize = 12.sp)
                        }
                        TextButton(onClick = { editingIndex = idx }) { Text("编辑", color = TextDim, fontSize = 12.sp) }
                        TextButton(onClick = {
                            list = list.filterIndexed { i, _ -> i != idx }
                            repo.save(list)
                        }) { Text("删除", color = TextDim, fontSize = 12.sp) }
                    }
                }
            }
        }
        if (list.isEmpty()) Text("还没有成绩记录", color = TextDim, modifier = Modifier.padding(top = 8.dp))
    }
    if (editingIndex >= 0 && editingIndex < list.size) {
        GradeEditDialog(
            grade = list[editingIndex],
            onSave = { updated ->
                val next = list.toMutableList()
                next[editingIndex] = updated
                list = next
                repo.save(list)
                editingIndex = -1
            },
            onDismiss = { editingIndex = -1 }
        )
    }
}

@Composable
private fun GradeEditDialog(grade: GradeRec, onSave: (GradeRec) -> Unit, onDismiss: () -> Unit) {
    var course by remember { mutableStateOf(grade.course) }
    var credit by remember { mutableStateOf(grade.credit.toString()) }
    var score by remember { mutableStateOf(grade.score.toString()) }
    var term by remember { mutableStateOf(grade.term) }
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp)) {
                Text("编辑成绩", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                OutlinedTextField(value = course, onValueChange = { course = it }, label = { Text("课程名") }, modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
                OutlinedTextField(value = term, onValueChange = { term = it }, label = { Text("学期（可空）") }, modifier = Modifier.fillMaxWidth())
                Row(Modifier.fillMaxWidth()) {
                    OutlinedTextField(value = credit, onValueChange = { credit = it }, label = { Text("学分") }, modifier = Modifier.weight(1f))
                    Spacer(Modifier.width(8.dp))
                    OutlinedTextField(value = score, onValueChange = { score = it }, label = { Text("成绩") }, modifier = Modifier.weight(1f))
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("取消", color = TextDim) }
                    Button(onClick = {
                        val c = credit.toDoubleOrNull()
                        val sc = score.toDoubleOrNull()
                        if (course.isNotBlank() && c != null && sc != null && c > 0 && sc >= 0 && sc <= 100) {
                            onSave(GradeRec(course.trim(), c, sc, term.trim()))
                        }
                    }) { Text("保存") }
                }
            }
        }
    }
}

@Composable
private fun DdlScreen(store: CtStore, onDone: (CtEvent) -> Unit) {
    val open = store.events.filter { it.ddl && !it.done }.sortedBy { it.end }
    val done = store.events.filter { it.ddl && it.done }.sortedByDescending { it.end }
    Row(
        Modifier.fillMaxSize().padding(10.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        BoardColumn("未完成 ${open.size}", open, onDone, Modifier.weight(1f))
        BoardColumn("已完成 ${done.size}", done, null, Modifier.weight(1f))
    }
}
@OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
@Composable
private fun EventRow(e: CtEvent, onEdit: (CtEvent) -> Unit, onLongPress: (CtEvent) -> Unit = {}) {
    Card(
        Modifier.fillMaxWidth().padding(vertical = 4.dp)
            .combinedClickable(
                onClick = { onEdit(e) },
                onLongClick = { onLongPress(e) }
            ),
        colors = CardDefaults.cardColors(containerColor = CardColor)
    ) {
        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(e.title, color = TextMain, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                val time = when {
                    e.isPoint() -> e.start.substringAfter('T')
                    e.start.isNotEmpty() && e.end.isNotEmpty() -> {
                        val sd = e.start.take(10)
                        val ed = e.end.take(10)
                        if (ed > sd) {
                            // 跨日：显示结束日期（如 22:00 → 10-26 08:00）
                            "${e.start.substringAfter('T')} → ${e.end.substring(5, 10)} ${e.end.substringAfter('T')}"
                        } else {
                            "${e.start.substringAfter('T')} – ${e.end.substringAfter('T')}"
                        }
                    }
                    else -> ""
                }
val datePart = if (e.start.length >= 10) e.start.substring(5, 10) else ""
                Text(
                    (if (datePart.isNotEmpty()) "$datePart " else "") + time + if (e.ddl) "  · DDL" else "",
                    color = if (e.ddl) Accent else TextDim, fontSize = 12.sp
                )
            }
            if (e.done) Text("✓", color = Color(0xFF4ADE80), fontSize = 16.sp)
        }
    }
}

@Composable
private fun SettingsScreen(repo: CalendarRepository, gpaRepo: GpaRepository, store: CtStore, onStore: (CtStore) -> Unit) {
    val ctx = LocalContext.current
    val prefs = remember { SyncPrefs(ctx) }
    var server by remember { mutableStateOf(prefs.server) }
    var user by remember { mutableStateOf(prefs.user) }
    var pass by remember { mutableStateOf("") }
    var token by remember { mutableStateOf(prefs.token) }
    var status by remember { mutableStateOf(if (prefs.token.isNotEmpty()) "已登录（已保存 token）" else "未登录") }
    val scope = rememberCoroutineScope()
    val client = remember { SyncClient(server) }
    var showTags by remember { mutableStateOf(false) }
    var showBackups by remember { mutableStateOf(false) }
    var showImport by remember { mutableStateOf(false) }
    var showSubs by remember { mutableStateOf(false) }
    var showBot by remember { mutableStateOf(false) }
    var showReminders by remember { mutableStateOf(false) }
    var showStats by remember { mutableStateOf(false) }
    var showExams by remember { mutableStateOf(false) }
    var showArchive by remember { mutableStateOf(false) }
    var showClearConfirm by remember { mutableStateOf(false) }
    var showPomodoro by remember { mutableStateOf(false) }
    var showHabits by remember { mutableStateOf(false) }

    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(12.dp)) {
        Text("账号同步", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        Text("先在桌面端开启同步服务，再在这里登录。", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(vertical = 8.dp))
        OutlinedTextField(value = server, onValueChange = { server = it; client.server = it; prefs.server = it }, label = { Text("服务器") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = user, onValueChange = { user = it; prefs.user = it }, label = { Text("账号") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = pass, onValueChange = { pass = it }, label = { Text("密码") }, modifier = Modifier.fillMaxWidth())
        Row(Modifier.padding(top = 8.dp)) {
            Button(onClick = {
                scope.launch(Dispatchers.IO) {
                    val r = client.login(user, pass)
                    withContext(Dispatchers.Main) {
                        status = if (r.ok) {
                            token = r.token
                            prefs.token = r.token
                            prefs.server = server
                            prefs.user = user
                            "已登录"
                        } else "登录失败：${r.err}"
                    }
                }
            }) { Text("登录") }
            Spacer(Modifier.width(8.dp))
            Button(onClick = {
                scope.launch(Dispatchers.IO) {
                    val r = client.register(user, pass)
                    withContext(Dispatchers.Main) {
                        status = if (r.ok) {
                            token = r.token
                            prefs.token = r.token
                            prefs.server = server
                            prefs.user = user
                            "已注册并登录"
                        } else "注册失败：${r.err}"
                    }
                }
            }) { Text("注册") }
        }
        Spacer(Modifier.height(8.dp))
        Row {
            Button(onClick = {
                scope.launch(Dispatchers.IO) {
                    val payload = try {
                        val root = JSONObject(store.toJsonString())
                        val arr = JSONArray()
                        for (g in gpaRepo.load()) {
                            arr.put(JSONObject().put("course", g.course).put("credit", g.credit).put("score", g.score))
                        }
                        root.put("grades", arr)
                        root.toString()
                    } catch (_: Exception) {
                        store.toJsonString()
                    }
                    val r = client.push(token, payload)
                    withContext(Dispatchers.Main) {
                        status = if (r.ok) "已上传（含 ${gpaRepo.load().size} 门成绩）" else "上传失败：${r.err}"
                    }
                }
            }) { Text("上传到服务器") }
            Spacer(Modifier.width(8.dp))
            Button(onClick = {
                scope.launch(Dispatchers.IO) {
                    val r = client.pull(token)
                    var gradesCount = 0
                    if (r.ok) {
                        try {
                            val grades = JSONObject(r.store).optJSONArray("grades")
                            if (grades != null) {
                                val list = mutableListOf<GradeRec>()
                                for (i in 0 until grades.length()) {
                                    val o = grades.optJSONObject(i) ?: continue
                                    val c = o.optString("course")
                                    val cr = o.optDouble("credit", 0.0)
                                    val sc = o.optDouble("score", 0.0)
                                    if (c.isNotEmpty() && cr > 0) list.add(GradeRec(c, cr, sc))
                                }
                                gpaRepo.save(list)
                                gradesCount = list.size
                            }
                        } catch (_: Exception) {
                            // 成绩解析失败不影响日程拉取
                        }
                    }
                    withContext(Dispatchers.Main) {
                        if (r.ok) {
                            val remote = CtStore.fromJsonString(r.store)
                            val localCount = store.events.size
                            // 与本地合并，而不是覆盖（避免本地独有日程丢失）
                            val merged = Merge.stores(store, remote)
                            repo.backupBeforeWrite()
                            repo.save(merged)
                            onStore(merged)
                            status = "已合并拉取（本地 $localCount + 服务器 ${remote.events.size} → ${merged.events.size} 条日程、${gradesCount} 门成绩）"
                        } else {
                            status = "拉取失败：${r.err}"
                        }
                    }
                }
            }) { Text("从服务器拉取") }
        }
        Text(status, color = if (status.contains("失败")) Color(0xFFFF6B7A) else Accent, modifier = Modifier.padding(top = 8.dp))
        Text("本地共 ${store.events.size} 条日程、${store.tags.size} 个标签", color = TextDim, fontSize = 11.sp)
        Spacer(Modifier.height(24.dp))
        Text("说明", color = TextMain, fontWeight = FontWeight.Bold)
        Text("• 同步为合并模式：同 id 事件按更新时间取较新，双方独有事件都会保留，不会整份覆盖。", color = TextDim, fontSize = 12.sp)
        Text("• 拉取/上传前会自动备份到 backup/。数据格式与桌面 data/calendar.json 一致。", color = TextDim, fontSize = 12.sp)
        Spacer(Modifier.height(24.dp))
        Text("数据", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        val icsLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                try {
                    val text = ctx.contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() } ?: ""
                    val imported = IcsClient.parse(text)
                    if (imported.isNotEmpty()) {
                        val list = store.events.toMutableList()
                        list.addAll(imported)
                        val next = store.copy(events = list)
                        repo.save(next)
                        onStore(next)
                        status = "已导入 ${imported.size} 条 ICS 日程"
                    } else {
                        status = "没有从文件里解析到日程"
                    }
                } catch (e: Exception) {
                    status = "导入失败：${e.message}"
                }
            }
        }
        Row(Modifier.padding(top = 6.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                try {
                    val ics = IcsClient.export(store.events)
                    val f = File(ctx.cacheDir, "caelendar.ics")
                    f.writeText(ics)
                    val uri = FileProvider.getUriForFile(ctx, "com.caelendar.app.fileprovider", f)
                    val share = Intent(Intent.ACTION_SEND).apply {
                        type = "text/calendar"
                        putExtra(Intent.EXTRA_STREAM, uri)
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                    ctx.startActivity(Intent.createChooser(share, "导出 ICS"))
                    status = "已生成 ICS（${store.events.size} 条）"
                } catch (e: Exception) {
                    status = "导出失败：${e.message}"
                }
            }) { Text("导出 ICS") }
            Button(onClick = { icsLauncher.launch(arrayOf("text/calendar", "text/*", "*/*")) }) { Text("导入 ICS") }
        }
        Text("每次保存前自动备份到 backup/（保留最近 10 份）", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
        val restoreLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                try {
                    val text = ctx.contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() } ?: ""
                    val restored = CtStore.fromJsonString(text)
                    if (restored.events.isNotEmpty() || restored.tags.isNotEmpty()) {
                        repo.save(restored)
                        onStore(restored)
                        status = "已恢复备份（${restored.events.size} 条日程）"
                    } else {
                        status = "备份文件里没有有效数据"
                    }
                } catch (e: Exception) {
                    status = "恢复失败：${e.message}"
                }
            }
        }
        Row(Modifier.padding(top = 6.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TextButton(onClick = {
                try {
                    val f = File(ctx.cacheDir, "calendar-backup.json")
                    f.writeText(store.toJsonString())
                    val uri = FileProvider.getUriForFile(ctx, "com.caelendar.app.fileprovider", f)
                    val share = Intent(Intent.ACTION_SEND).apply {
                        type = "application/json"
                        putExtra(Intent.EXTRA_STREAM, uri)
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                    ctx.startActivity(Intent.createChooser(share, "导出数据备份"))
                    status = "已生成数据备份（${store.events.size} 条日程）"
                } catch (e: Exception) {
                    status = "导出失败：${e.message}"
                }
            }) { Text("导出备份", color = TextDim, fontSize = 12.sp) }
            TextButton(onClick = { restoreLauncher.launch(arrayOf("application/json", "text/*", "*/*")) }) {
                Text("恢复备份", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showBackups = true }) {
                Text("备份管理", color = TextDim, fontSize = 12.sp)
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TextButton(onClick = { showImport = true }) {
                Text("批量导入", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showSubs = true }) {
                Text("订阅 ICS", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = {
                val txt = buildBusyFreeText(store.events)
                try {
                    val share = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_TEXT, txt)
                    }
                    ctx.startActivity(Intent.createChooser(share, "分享忙闲"))
                } catch (e: Exception) {
                    status = "分享失败：${e.message}"
                }
            }) {
                Text("忙闲分享", color = TextDim, fontSize = 12.sp)
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TextButton(onClick = {
                try {
                    val html = buildPlanHtml(store)
                    val f = File(ctx.cacheDir, "study-plan.html")
                    f.writeText(html)
                    val uri = FileProvider.getUriForFile(ctx, "com.caelendar.app.fileprovider", f)
                    val share = Intent(Intent.ACTION_SEND).apply {
                        type = "text/html"
                        putExtra(Intent.EXTRA_STREAM, uri)
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                    ctx.startActivity(Intent.createChooser(share, "分享学习计划"))
                    status = "已生成未来 7 天学习计划"
                } catch (e: Exception) {
                    status = "导出失败：${e.message}"
                }
            }) { Text("导出计划", color = TextDim, fontSize = 12.sp) }
            TextButton(onClick = { showArchive = true }) {
                Text("存档管理", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showClearConfirm = true }) {
                Text("清空日程", color = Color(0xFFFF6B7A), fontSize = 12.sp)
            }
            TextButton(onClick = {
                if (ShareServer.isRunning()) {
                    ShareServer.stop()
                    status = "局域网分享已停止"
                } else {
                    if (ShareServer.start(repo)) {
                        status = "局域网分享已开启：" + ShareServer.url()
                    } else {
                        status = "开启失败（端口 8787 被占用？）"
                    }
                }
            }) {
                Text(
                    if (ShareServer.isRunning()) "停止分享" else "局域网分享",
                    color = if (ShareServer.isRunning()) Accent else TextDim,
                    fontSize = 12.sp
                )
            }
        }
        if (ShareServer.isRunning()) {
            Text(
                "同一 WiFi 下打开：${ShareServer.url()}",
                color = Accent, fontSize = 11.sp, modifier = Modifier.padding(top = 2.dp)
            )
        }
        val storagePrefs = remember { ctx.getSharedPreferences("caelendar_storage", android.content.Context.MODE_PRIVATE) }
        var engine by remember { mutableStateOf(storagePrefs.getString("engine", "json") ?: "json") }
        Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("存储引擎", color = TextMain, fontSize = 14.sp)
                Text(
                    if (engine == "sqlite") {
                        "SQLite（读走数据库，写双写 JSON 备份）" +
                            runCatching {
                                val f = File(ctx.filesDir, "calendar.db")
                                if (f.exists()) " · ${f.length() / 1024} KB" else ""
                            }.getOrDefault("")
                    } else {
                        "JSON 文件（与桌面 calendar.json 一致）"
                    },
                    color = TextDim, fontSize = 11.sp
                )
            }
            TextButton(onClick = {
                val next = if (engine == "sqlite") "json" else "sqlite"
                if (next == "sqlite") {
                    try {
                        val db = com.caelendar.app.data.SqliteStore(ctx)
                        db.migrateFromJson(store)
                        repo.sqlite = db
                    } catch (e: Exception) {
                        status = "切换失败：${e.message}"
                        return@TextButton
                    }
                } else {
                    repo.sqlite = null
                }
                storagePrefs.edit().putString("engine", next).apply()
                engine = next
                status = if (next == "sqlite") "已切到 SQLite 引擎" else "已切回 JSON 引擎"
            }) { Text(if (engine == "sqlite") "切回 JSON" else "切到 SQLite", color = Accent, fontSize = 12.sp) }
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("喝水提醒", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("08:00–22:30 每 45 分钟提醒，通知可一键 +1 杯", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = {
                val on = !WaterReminder.isEnabled(ctx)
                WaterReminder.setEnabled(ctx, on)
                status = if (on) "喝水提醒已开启" else "喝水提醒已关闭"
            }) {
                Text(if (WaterReminder.isEnabled(ctx)) "关闭" else "开启", color = if (WaterReminder.isEnabled(ctx)) Accent else TextDim)
            }
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("提醒中心", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("查看未来 7 天提醒，一键完成", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showReminders = true }) { Text("打开", color = Accent) }
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("统计", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("近 7 天按标签统计时长", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showStats = true }) { Text("查看", color = Accent) }
        }
        if (showStats) {
            StatisticsDialog(events = store.events, tags = store.tags, water = store.waterMap(), onDismiss = { showStats = false })
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("考试倒计时", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("未来 90 天的考试还剩几天", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showExams = true }) { Text("查看", color = Accent) }
        }
        if (showExams) {
            ExamCountdownDialog(events = store.events, tags = store.tags, onDismiss = { showExams = false })
        }
        if (showClearConfirm) {
            androidx.compose.material3.AlertDialog(
                onDismissRequest = { showClearConfirm = false },
                title = { Text("清空所有日程", color = TextMain, fontWeight = FontWeight.Bold) },
                text = {
                    Text(
                        "将删除全部 ${store.events.size} 条日程并清空回收站（保存前会自动备份到 backup/）。\n" +
                            "标签、主题、喝水记录、设置都保留。",
                        color = TextMain
                    )
                },
                confirmButton = {
                    TextButton(onClick = {
                        val n = store.events.size
                        val cleared = store.copy(events = mutableListOf(), tombstones = mutableListOf())
                        repo.save(cleared)
                        onStore(cleared)
                        status = "已清空 $n 条日程（可从备份恢复）"
                        showClearConfirm = false
                    }) { Text("清空", color = Color(0xFFFF6B7A)) }
                },
                dismissButton = {
                    TextButton(onClick = { showClearConfirm = false }) { Text("取消", color = TextDim) }
                }
            )
        }
        if (showArchive) {
            ArchiveDialog(
                repo = repo,
                store = store,
                onLoad = { st ->
                    onStore(st)
                    status = "已载入存档（${st.events.size} 条日程）"
                },
                onDismiss = { showArchive = false }
            )
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("番茄钟", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("专注计时 · 今日记录", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showPomodoro = true }) { Text("打开", color = Accent) }
        }
        if (showPomodoro) {
            PomodoroDialog(onDismiss = { showPomodoro = false })
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("小组件样式", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("今日 / 本周 / 时钟", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = {
                val sp = ctx.getSharedPreferences("caelendar_widget", android.content.Context.MODE_PRIVATE)
                val cur = sp.getString("mode", "today") ?: "today"
                val nextMode = when (cur) {
                    "today" -> "week"
                    "week" -> "clock"
                    else -> "today"
                }
                sp.edit().putString("mode", nextMode).apply()
                val mgr = android.appwidget.AppWidgetManager.getInstance(ctx)
                val ids = mgr.getAppWidgetIds(android.content.ComponentName(ctx, com.caelendar.app.widget.CalendarWidgetProvider::class.java))
                if (ids.isNotEmpty()) {
                    com.caelendar.app.widget.CalendarWidgetProvider().onUpdate(ctx, mgr, ids)
                }
                status = "小组件样式：" + when (nextMode) {
                    "week" -> "本周"
                    "clock" -> "时钟"
                    else -> "今日"
                }
            }) {
                val cur = ctx.getSharedPreferences("caelendar_widget", android.content.Context.MODE_PRIVATE)
                    .getString("mode", "today") ?: "today"
                Text(
                    when (cur) {
                        "week" -> "本周"
                        "clock" -> "时钟"
                        else -> "今日"
                    },
                    color = Accent
                )
            }
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("机器人（Telegram）", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("绑定 Bot 后可查询今日/未来 7 天日程", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showBot = true }) { Text("配置", color = Accent) }
        }
        Spacer(Modifier.height(12.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("习惯打卡", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("自定义习惯 · 每日打卡 · 连续天数", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showHabits = true }) { Text("打开", color = Accent) }
        }
        if (showHabits) {
            HabitsDialog(onDismiss = { showHabits = false })
        }
        if (showReminders) {
            ReminderCenterDialog(
                events = store.events,
                onDone = { e ->
                    val list = store.events.toMutableList()
                    val idx = list.indexOfFirst { it.id == e.id }
                    if (idx >= 0) {
                        list[idx] = list[idx].copy(done = true)
                        val next = store.copy(events = list)
                        repo.save(next)
                        onStore(next)
                    }
                },
                onDismiss = { showReminders = false }
            )
        }
        Spacer(Modifier.height(24.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("标签管理", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text("共 ${store.tags.size} 个标签", color = TextDim, fontSize = 12.sp)
            }
            TextButton(onClick = { showTags = true }) { Text("管理", color = Accent) }
        }
        if (showImport) {
        TextImportDialog(
            onImport = { list ->
                val next = store.events.toMutableList()
                next.addAll(list)
                val updated = store.copy(events = next)
                onStore(updated)
                repo.save(updated)
                showImport = false
            },
            onDismiss = { showImport = false }
        )
    }
    if (showBot) {
        BotDialog(
            store = store,
            onDismiss = { showBot = false }
        )
    }
    if (showSubs) {
        SubscriptionDialog(
            store = store,
            onImported = { added ->
                if (added.isNotEmpty()) {
                    val next = store.events.toMutableList()
                    next.addAll(added)
                    val updated = store.copy(events = next)
                    onStore(updated)
                    repo.save(updated)
                }
                showSubs = false
            },
            onDismiss = { showSubs = false }
        )
    }
    if (showBackups) {
        BackupManagerDialog(
            repo = repo,
            onRestored = { restored ->
                onStore(restored)
                showBackups = false
            },
            onDismiss = { showBackups = false }
        )
    }
    if (showTags) {
            TagManagerDialog(store, onStore = onStore, onDismiss = { showTags = false })
        }
        Spacer(Modifier.height(24.dp))
        Text("外观", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = { ThemeState.dark = true; ThemeState.save(ctx) },
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (ThemeState.dark) Accent else CardColor,
                    contentColor = if (ThemeState.dark) Color(0xFF141824) else TextMain
                )
            ) { Text("深色") }
            Button(
                onClick = { ThemeState.dark = false; ThemeState.save(ctx) },
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (!ThemeState.dark) Accent else CardColor,
                    contentColor = if (!ThemeState.dark) Color(0xFF141824) else TextMain
                )
            ) { Text("浅色") }
        }
        Text("主题预设", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ThemeState.presets.forEach { preset ->
                val on = ThemeState.accentIndex == preset.accent && ThemeState.dark == preset.dark
                Box(
                    Modifier
                        .background(if (on) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                        .clickable {
                            ThemeState.accentIndex = preset.accent
                            ThemeState.dark = preset.dark
                            ThemeState.save(ctx)
                        }
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(preset.name, color = if (on) Accent else TextDim, fontSize = 11.sp)
                }
            }
        }
        Text("字号", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf(0.9f to "小", 1.0f to "标准", 1.15f to "大", 1.3f to "特大").forEach { (scale, label) ->
                val on = kotlin.math.abs(ThemeState.fontScale - scale) < 0.01f
                Box(
                    Modifier
                        .background(if (on) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                        .clickable {
                            ThemeState.fontScale = scale
                            ThemeState.save(ctx)
                        }
                        .padding(horizontal = 10.dp, vertical = 4.dp)
                ) {
                    Text(label, color = if (on) Accent else TextDim, fontSize = 12.sp)
                }
            }
        }
        Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("高对比度", color = TextMain, fontSize = 13.sp)
            Spacer(Modifier.weight(1f))
            TextButton(onClick = {
                ThemeState.highContrast = !ThemeState.highContrast
                ThemeState.save(ctx)
            }) { Text(if (ThemeState.highContrast) "已开启" else "关闭", color = if (ThemeState.highContrast) Accent else TextDim) }
        }
        Text("强调色", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 8.dp))
        Row(Modifier.fillMaxWidth().padding(top = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            var customHex by remember { mutableStateOf(ThemeState.customAccentHex) }
            OutlinedTextField(
                value = customHex, onValueChange = { customHex = it },
                label = { Text("自定义强调色 #RRGGBB") },
                singleLine = true,
                modifier = Modifier.weight(1f)
            )
            Spacer(Modifier.width(6.dp))
            TextButton(onClick = {
                val hex = customHex.trim()
                if (hex.length == 7 && hex.startsWith("#")) {
                    ThemeState.customAccentHex = hex
                    ThemeState.save(ctx)
                    status = "强调色已设为 $hex"
                } else {
                    status = "格式应为 #RRGGBB"
                }
            }) { Text("应用", color = Accent, fontSize = 12.sp) }
            if (ThemeState.customAccentHex.isNotEmpty()) {
                TextButton(onClick = {
                    ThemeState.customAccentHex = ""
                    ThemeState.save(ctx)
                    status = "已恢复预设强调色"
                }) { Text("清除", color = TextDim, fontSize = 12.sp) }
            }
        }
        Row(Modifier.padding(top = 4.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            ThemeState.accents.forEachIndexed { idx, c ->
                Box(
                    Modifier
                        .size(36.dp)
                        .background(c, androidx.compose.foundation.shape.CircleShape)
                        .then(
                            if (ThemeState.accentIndex == idx) Modifier.border(3.dp, TextMain, androidx.compose.foundation.shape.CircleShape)
                            else Modifier
                        )
                        .clickable { ThemeState.accentIndex = idx; ThemeState.save(ctx) },
                    contentAlignment = Alignment.Center
                ) {
                    if (ThemeState.accentIndex == idx) Text("✓", color = Color(0xFF141824), fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
        Spacer(Modifier.height(24.dp))
        Text("关于", color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        Text("星历 CAELENDAR · Android ${BuildConfig.VERSION_NAME}", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 6.dp))
        Text("数据格式 format 2，与桌面 data/calendar.json 一致", color = TextDim, fontSize = 12.sp)
        Text("单机 + 局域网账号同步 · 提醒通知 · 桌面小组件", color = TextDim, fontSize = 12.sp)
    }
}

private fun EventMinutes(e: CtEvent): Int {
    val s = e.startTime() ?: return 0
    val en = e.endTime() ?: return 0
    return java.time.Duration.between(s, en).toMinutes().toInt().coerceIn(0, 24 * 60)
}

@Composable
private fun HabitsDialog(onDismiss: () -> Unit) {
    val ctx = LocalContext.current
    val store = remember { HabitsStore(ctx) }
    var habits by remember { mutableStateOf(store.loadHabits()) }
    var checkins by remember { mutableStateOf(store.loadCheckins()) }
    var name by remember { mutableStateOf("") }
    val today = LocalDate.now()

    fun persistCheckins() {
        store.saveCheckins(checkins)
    }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 560.dp)) {
                Text("习惯打卡", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("连续天数从今天往前算（今天没打则从昨天数）", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = name, onValueChange = { name = it },
                        label = { Text("新习惯") },
                        modifier = Modifier.weight(1f)
                    )
                    Spacer(Modifier.width(6.dp))
                    Button(onClick = {
                        if (name.isNotBlank()) {
                            val next = (habits + CtHabit("habit-" + UUID.randomUUID().toString().take(8), name.trim())).toMutableList()
                            habits = next
                            store.saveHabits(next)
                            name = ""
                        }
                    }) { Text("添加") }
                }
                Column(Modifier.fillMaxWidth().heightIn(max = 300.dp).verticalScroll(rememberScrollState()).padding(top = 8.dp)) {
                    if (habits.isEmpty()) {
                        Text("还没有习惯，先添加一个", color = TextDim, modifier = Modifier.padding(top = 8.dp))
                    }
                    habits.forEach { h ->
                        val set = checkins[h.id] ?: mutableSetOf()
                        val checkedToday = set.contains(today.toString())
                        Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(h.name, color = TextMain, fontSize = 14.sp)
                                Text("连续 ${store.streak(set, today)} 天", color = TextDim, fontSize = 11.sp)
                                Row(Modifier.padding(top = 3.dp), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                                    for (i in 6 downTo 0) {
                                        val d = today.minusDays(i.toLong())
                                        val on = set.contains(d.toString())
                                        Box(
                                            Modifier.size(10.dp).background(
                                                if (on) Accent else CardColor,
                                                RoundedCornerShape(50)
                                            )
                                        )
                                    }
                                }
                            }
                            TextButton(onClick = {
                                val nextSet = set.toMutableSet()
                                if (checkedToday) nextSet.remove(today.toString()) else nextSet.add(today.toString())
                                checkins = (checkins.toMutableMap().apply { put(h.id, nextSet) })
                                persistCheckins()
                            }) {
                                Text(if (checkedToday) "已打卡 ✓" else "打卡", color = if (checkedToday) Accent else TextDim, fontSize = 12.sp)
                            }
                            TextButton(onClick = {
                                val nextHabits = habits.filter { it.id != h.id }.toMutableList()
                                habits = nextHabits
                                store.saveHabits(nextHabits)
                                checkins = checkins.toMutableMap().apply { remove(h.id) }
                                persistCheckins()
                            }) { Text("删除", color = TextDim, fontSize = 11.sp) }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun PomodoroDialog(onDismiss: () -> Unit) {
    val ctx = LocalContext.current
    var totalMin by remember { mutableStateOf(25) }
    var remaining by remember { mutableStateOf(25 * 60) }
    var running by remember { mutableStateOf(false) }
    var doneToday by remember { mutableStateOf(0) }
    var minutesToday by remember { mutableStateOf(0) }

    fun loadStats() {
        val sp = ctx.getSharedPreferences("caelendar_focus", android.content.Context.MODE_PRIVATE)
        val today = LocalDate.now().toString()
        val raw = sp.getString("records", "") ?: ""
        var c = 0
        var m = 0
        if (raw.isNotEmpty()) {
            try {
                val arr = JSONArray(raw)
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    if (o.optString("date") == today) {
                        c++
                        m += o.optInt("minutes")
                    }
                }
            } catch (_: Exception) {
            }
        }
        doneToday = c
        minutesToday = m
    }

    fun saveRecord(minutes: Int) {
        val sp = ctx.getSharedPreferences("caelendar_focus", android.content.Context.MODE_PRIVATE)
        val raw = sp.getString("records", "") ?: ""
        val arr = try { if (raw.isEmpty()) JSONArray() else JSONArray(raw) } catch (_: Exception) { JSONArray() }
        val o = JSONObject()
        o.put("date", LocalDate.now().toString())
        o.put("minutes", minutes)
        arr.put(o)
        sp.edit().putString("records", arr.toString()).apply()
        loadStats()
    }

    LaunchedEffect(running, totalMin) {
        while (running && remaining > 0) {
            kotlinx.coroutines.delay(1000)
            remaining -= 1
        }
        if (running && remaining <= 0) {
            running = false
            saveRecord(totalMin)
        }
    }

    LaunchedEffect(Unit) { loadStats() }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(20.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text("番茄钟", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("今天 $doneToday 次 · $minutesToday 分钟", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 4.dp))
                Text(
                    "%02d:%02d".format(remaining / 60, remaining % 60),
                    color = Accent, fontSize = 44.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier.padding(vertical = 10.dp)
                )
                androidx.compose.material3.LinearProgressIndicator(
                    progress = 1f - remaining.toFloat() / (totalMin * 60).toFloat(),
                    modifier = Modifier.fillMaxWidth().height(6.dp),
                    color = Accent
                )
                Row(Modifier.padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(15, 25, 45).forEach { m ->
                        Box(
                            Modifier
                                .background(if (totalMin == m && !running) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                .clickable(enabled = !running) {
                                    totalMin = m
                                    remaining = m * 60
                                }
                                .padding(horizontal = 12.dp, vertical = 4.dp)
                        ) {
                            Text("${m}分", color = if (totalMin == m) Accent else TextDim, fontSize = 12.sp)
                        }
                    }
                }
                Row(Modifier.padding(top = 14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Button(onClick = { running = !running }) { Text(if (running) "暂停" else "开始") }
                    TextButton(onClick = {
                        running = false
                        remaining = totalMin * 60
                    }) { Text("重置", color = TextDim) }
                }
                Row(Modifier.fillMaxWidth().padding(top = 6.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun BatchDialog(
    day: LocalDate,
    events: List<CtEvent>,
    tags: List<CtTag>,
    onApply: (Set<String>, String, List<String>) -> Unit,
    onDismiss: () -> Unit
) {
    val selected = remember { mutableStateListOf<String>() }
    val tagSel = remember { mutableStateListOf<String>() }
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 560.dp)) {
                Text("多选操作 · $day", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("共 ${events.size} 项，已选 ${selected.size}", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 2.dp))
                Row(Modifier.fillMaxWidth().padding(top = 6.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = {
                        if (selected.size == events.size) selected.clear()
                        else { selected.clear(); selected.addAll(events.map { it.id }) }
                    }, contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 6.dp, vertical = 2.dp)) {
                        Text(if (selected.size == events.size) "取消全选" else "全选", color = Accent, fontSize = 12.sp)
                    }
                }
                Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState())) {
                    if (events.isEmpty()) Text("这一天没有日程", color = TextDim, modifier = Modifier.padding(top = 8.dp))
                    for (e in events) {
                        Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                            Checkbox(
                                checked = e.id in selected,
                                onCheckedChange = { on ->
                                    if (on) selected.add(e.id) else selected.remove(e.id)
                                }
                            )
                            Column(Modifier.weight(1f)) {
                                Text(e.title.ifEmpty { "未命名" }, color = TextMain, fontSize = 13.sp, maxLines = 1)
                                Text(
                                    (if (e.allDay) "全天" else e.start.substringAfter('T')) +
                                        (if (e.location.isNotEmpty()) " · ${e.location}" else ""),
                                    color = TextDim, fontSize = 10.sp
                                )
                            }
                        }
                    }
                }
                Text("加标签时附加到选中项（可多选）", color = TextDim, fontSize = 10.sp, modifier = Modifier.padding(top = 6.dp))
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (t in tags.take(12)) {
                        val on = t.id in tagSel
                        Box(
                            Modifier
                                .background(if (on) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                .clickable { if (on) tagSel.remove(t.id) else tagSel.add(t.id) }
                                .padding(horizontal = 8.dp, vertical = 3.dp)
                        ) { Text(t.name, color = if (on) Accent else TextDim, fontSize = 11.sp) }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = { if (selected.isNotEmpty()) onApply(selected.toSet(), "done", emptyList()) }) {
                        Text("标记完成", color = Accent, fontSize = 12.sp)
                    }
                    TextButton(onClick = { if (selected.isNotEmpty()) onApply(selected.toSet(), "delete", emptyList()) }) {
                        Text("删除", color = Color(0xFFFF6B7A), fontSize = 12.sp)
                    }
                    TextButton(onClick = { if (selected.isNotEmpty()) onApply(selected.toSet(), "tag", tagSel.toList()) }) {
                        Text("加标签", color = Accent, fontSize = 12.sp)
                    }
                    Spacer(Modifier.weight(1f))
                    TextButton(onClick = onDismiss) { Text("关闭", color = TextDim, fontSize = 12.sp) }
                }
            }
        }
    }
}

@Composable
private fun ArchiveDialog(
    repo: CalendarRepository,
    store: CtStore,
    onLoad: (CtStore) -> Unit,
    onDismiss: () -> Unit
) {
    var name by remember { mutableStateOf("") }
    var archives by remember { mutableStateOf(repo.listArchives()) }
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 520.dp)) {
                Text("存档管理", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("保存当前数据为独立存档，可随时载入", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 2.dp))
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = name, onValueChange = { name = it },
                        label = { Text("存档名（如：2026秋）") },
                        singleLine = true,
                        modifier = Modifier.weight(1f)
                    )
                    Spacer(Modifier.width(6.dp))
                    Button(onClick = {
                        val n = name.ifBlank { "存档-" + java.text.SimpleDateFormat("MMdd-HHmm", java.util.Locale.US).format(java.util.Date()) }
                        repo.saveArchive(n, store)
                        archives = repo.listArchives()
                        name = ""
                    }) { Text("保存") }
                }
                Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState()).padding(top = 6.dp)) {
                    if (archives.isEmpty()) {
                        Text("还没有存档", color = TextDim, modifier = Modifier.padding(top = 6.dp))
                    }
                    for (f in archives) {
                        Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(f.nameWithoutExtension, color = TextMain, fontSize = 13.sp)
                                Text("${f.length() / 1024} KB", color = TextDim, fontSize = 10.sp)
                            }
                            TextButton(onClick = {
                                repo.loadArchive(f)?.let { onLoad(it) }
                                archives = repo.listArchives()
                            }) { Text("载入", color = Accent, fontSize = 12.sp) }
                            TextButton(onClick = {
                                f.delete()
                                archives = repo.listArchives()
                            }) { Text("删除", color = Color(0xFFFF6B7A), fontSize = 12.sp) }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = Accent) }
                }
            }
        }
    }
}

@Composable
private fun ExamCountdownDialog(events: List<CtEvent>, tags: List<CtTag>, onDismiss: () -> Unit) {
    val today = LocalDate.now()
    val examTagIds = tags.filter { it.id == "exam" || it.name.contains("考试") }.map { it.id }.toSet()
    val exams = ExpandEvents(events, today, today.plusDays(90))
        .filter { e ->
            val byTag = e.tags.any { it in examTagIds }
            val byTitle = listOf("考试", "期中", "期末", "测验", "月考", "补考", "考核").any { e.title.contains(it) }
            byTag || byTitle
        }
        .sortedBy { it.start }
        .distinctBy { it.title + it.start.take(10) }
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 480.dp)) {
                Text("考试倒计时", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("未来 90 天 · ${exams.size} 场", color = TextDim, fontSize = 12.sp, modifier = Modifier.padding(top = 2.dp, bottom = 6.dp))
                Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState())) {
                    if (exams.isEmpty()) {
                        Text("未来 90 天没有考试日程", color = TextDim, modifier = Modifier.padding(top = 8.dp))
                    }
                    for (e in exams) {
                        val d = runCatching { LocalDate.parse(e.start.take(10)) }.getOrNull() ?: continue
                        val days = java.time.temporal.ChronoUnit.DAYS.between(today, d)
                        Row(Modifier.fillMaxWidth().padding(vertical = 5.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(e.title.ifEmpty { "未命名" }, color = TextMain, fontSize = 14.sp)
                                Text(
                                    "${d.monthValue}月${d.dayOfMonth}日 " +
                                        (if (e.start.length >= 16) e.start.substring(11, 16) else "") +
                                        (if (e.location.isNotEmpty()) " · ${e.location}" else ""),
                                    color = TextDim, fontSize = 11.sp
                                )
                            }
                            Text(
                                when {
                                    days == 0L -> "今天"
                                    days == 1L -> "明天"
                                    days < 0 -> "已过"
                                    else -> "$days 天"
                                },
                                color = when {
                                    days <= 3 -> Color(0xFFFF6B7A)
                                    days <= 14 -> Color(0xFFFFB454)
                                    else -> Accent
                                },
                                fontSize = 15.sp, fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭", color = Accent) }
                }
            }
        }
    }
}

@Composable
private fun StatisticsDialog(
    events: List<CtEvent>,
    tags: List<CtTag>,
    water: Map<String, Int> = emptyMap(),
    onDismiss: () -> Unit
) {
    var range by remember { mutableStateOf(0) }
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
                Text("统计", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), modifier = Modifier.padding(top = 4.dp)) {
                    listOf(0 to "近 7 天", 1 to "近 30 天", 2 to "全部").forEach { (i, label) ->
                        Box(
                            Modifier
                                .background(if (range == i) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                .clickable { range = i }
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(label, color = if (range == i) Accent else TextDim, fontSize = 12.sp)
                        }
                    }
                }

                val now = java.time.LocalDate.now()
                val from = when (range) {
                    0 -> now.minusDays(6)
                    1 -> now.minusDays(29)
                    else -> java.time.LocalDate.of(1970, 1, 1)
                }
                val recent = events.filter { e ->
                    e.start.length >= 10 && !e.allDay && runCatching {
                        val d = java.time.LocalDate.parse(e.start.substring(0, 10))
                        !d.isBefore(from) && !d.isAfter(now)
                    }.getOrDefault(false)
                }
                val totalMin = recent.sumOf { EventMinutes(it) }
                Text(
                    "共 ${recent.size} 项日程 · %.1f 小时".format(totalMin / 60.0),
                    color = Accent, fontSize = 13.sp, modifier = Modifier.padding(vertical = 8.dp)
                )
                // 本周喝水统计
                run {
                    val weekStart = LocalDate.now().minusDays(6)
                    var total = 0
                    var hitDays = 0
                    for (i in 0 until 7) {
                        val key = weekStart.plusDays(i.toLong()).toString()
                        val c2 = water[key] ?: 0
                        total += c2
                        if (c2 >= 8) hitDays++
                    }
                    Text(
                        "本周喝水 $total 杯 · 达标 $hitDays 天",
                        color = if (hitDays > 0) Color(0xFF4FC3F7) else TextDim,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(bottom = 6.dp)
                    )
                }
                val doneCount = recent.count { it.done }
                Text(
                    "完成 ${doneCount}/${recent.size}" + if (recent.isNotEmpty()) " · ${doneCount * 100 / recent.size}%" else "",
                    color = if (doneCount > 0) Color(0xFF4ADE80) else TextDim, fontSize = 12.sp,
                    modifier = Modifier.padding(bottom = 4.dp)
                )

                val perTag = linkedMapOf<String, Int>()
                for (e in recent) {
                    if (e.tags.isEmpty()) perTag["无标签"] = (perTag["无标签"] ?: 0) + EventMinutes(e)
                    for (tid in e.tags) {
                        val name = tags.firstOrNull { it.id == tid }?.name ?: tid
                        perTag[name] = (perTag[name] ?: 0) + EventMinutes(e)
                    }
                }
                val maxMin = perTag.values.maxOrNull()?.coerceAtLeast(1) ?: 1
                if (perTag.isEmpty()) {
                    Text("最近 7 天还没有日程", color = TextDim, modifier = Modifier.padding(top = 8.dp))
                } else {
                    // 饼图（按标签时长，环形）
                    run {
                        val palette = listOf(
                            Color(0xFFE3C177), Color(0xFF6EA8FF), Color(0xFF7ED9A0),
                            Color(0xFFC084FC), Color(0xFFF472B6), Color(0xFFFFB454),
                            Color(0xFF4FD1C5), Color(0xFFFF7A85)
                        )
                        val totalMin2 = perTag.values.sum().coerceAtLeast(1)
                        val entries = perTag.entries.sortedByDescending { it.value }
                        androidx.compose.foundation.Canvas(
                            Modifier.fillMaxWidth().height(150.dp).padding(top = 8.dp)
                        ) {
                            val radius = size.minDimension * 0.32f
                            val centerPt = androidx.compose.ui.geometry.Offset(size.width / 2f, size.height / 2f)
                            var startAngle = -90f
                            entries.forEachIndexed { i, (_, mins) ->
                                val sweep = mins.toFloat() / totalMin2 * 360f
                                drawArc(
                                    color = palette[i % palette.size],
                                    startAngle = startAngle,
                                    sweepAngle = (sweep - 1.2f).coerceAtLeast(0.6f),
                                    useCenter = true,
                                    topLeft = androidx.compose.ui.geometry.Offset(centerPt.x - radius, centerPt.y - radius),
                                    size = androidx.compose.ui.geometry.Size(radius * 2, radius * 2)
                                )
                                startAngle += sweep
                            }
                            // 中心挖空成环形
                            drawCircle(color = CardColor, radius = radius * 0.52f, center = centerPt)
                        }
                    }
                    perTag.entries.sortedByDescending { it.value }.forEach { (name, mins) ->
                        Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                            Text(name, color = TextMain, fontSize = 12.sp, modifier = Modifier.width(64.dp))
                            Box(
                                Modifier.weight(1f).height(14.dp)
                                    .background(Accent.copy(alpha = 0.25f), RoundedCornerShape(7.dp))
                            ) {
                                Box(
                                    Modifier.fillMaxWidth((mins.toFloat() / maxMin).coerceIn(0.05f, 1f))
                                        .height(14.dp)
                                        .background(Accent.copy(alpha = 0.75f), RoundedCornerShape(7.dp))
                                )
                            }
                            Text(
                                "%.1fh".format(mins / 60.0), color = TextDim, fontSize = 11.sp,
                                modifier = Modifier.padding(start = 6.dp).width(46.dp)
                            )
                        }
                    }
                }

                // 本周每日事件数
                Text("本周每日", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 12.dp))
                Row(Modifier.fillMaxWidth().padding(top = 4.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    val monday = now.minusDays((now.dayOfWeek.value - 1).toLong())
                    for (i in 0 until 7) {
                        val d = monday.plusDays(i.toLong())
                        val cnt = events.count { it.start.startsWith(d.toString()) }
                        val h = (cnt * 6).coerceIn(4, 60)
                        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                            Box(
                                Modifier.height(h.dp).fillMaxWidth(0.55f)
                                    .background(if (cnt > 0) Accent.copy(alpha = 0.7f) else CardColor, RoundedCornerShape(3.dp))
                            )
                            Text("${d.dayOfMonth}", color = TextDim, fontSize = 9.sp)
                        }
                    }
                }

                Row(Modifier.fillMaxWidth().padding(top = 10.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun ReminderCenterDialog(
    events: List<CtEvent>,
    onDone: (CtEvent) -> Unit,
    onDismiss: () -> Unit
) {
    val today = LocalDate.now().toString()
    val upcoming = events.filter { e ->
        !e.done && e.start.length >= 16 && e.start.take(10) >= today
    }.sortedBy { it.start }.take(20)
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 520.dp)) {
                Text("提醒中心", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("未来 7 天 · 共 ${upcoming.size} 条", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                if (upcoming.isEmpty()) {
                    Text("接下来没有提醒", color = TextDim, modifier = Modifier.padding(top = 12.dp))
                } else {
                    Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState()).padding(top = 8.dp)) {
                        upcoming.forEach { e ->
                            Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                                Column(Modifier.weight(1f)) {
                                    Text(e.title.ifEmpty { "未命名" }, color = TextMain, fontSize = 13.sp)
                                    val lead = e.alarms.firstOrNull()
                                    Text(
                                        e.start.substring(5, 16).replace('T', ' ') +
                                            (if (e.ddl) " · DDL" else "") +
                                            (if (lead != null && lead >= 0) " · 提前${lead}分" else ""),
                                        color = TextDim, fontSize = 11.sp
                                    )
                                }
                                TextButton(onClick = { onDone(e) }) { Text("完成", color = Accent, fontSize = 11.sp) }
                            }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun BulkAddDialog(
    defaultDate: LocalDate,
    onCreate: (List<CtEvent>) -> Unit,
    onDismiss: () -> Unit
) {
    var title by remember { mutableStateOf("") }
    var startDate by remember { mutableStateOf(defaultDate.toString()) }
    var days by remember { mutableStateOf("3") }
    var time1 by remember { mutableStateOf("09:00") }
    var time2 by remember { mutableStateOf("10:00") }
    var skipWeekend by remember { mutableStateOf(true) }
    var hint by remember { mutableStateOf("") }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).verticalScroll(rememberScrollState())) {
                Text("批量添加", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("这段时间每天生成一条日程", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                OutlinedTextField(value = title, onValueChange = { title = it }, label = { Text("标题") }, modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
                OutlinedTextField(value = startDate, onValueChange = { startDate = it }, label = { Text("开始日期 YYYY-MM-DD") }, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(value = days, onValueChange = { days = it }, label = { Text("天数（1-30）") }, modifier = Modifier.fillMaxWidth())
                Row(Modifier.fillMaxWidth()) {
                    OutlinedTextField(value = time1, onValueChange = { time1 = it }, label = { Text("每天开始") }, modifier = Modifier.weight(1f))
                    Spacer(Modifier.width(8.dp))
                    OutlinedTextField(value = time2, onValueChange = { time2 = it }, label = { Text("每天结束") }, modifier = Modifier.weight(1f))
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Checkbox(checked = skipWeekend, onCheckedChange = { skipWeekend = it })
                    Text("跳过周末", color = TextMain, fontSize = 13.sp)
                }
                if (hint.isNotEmpty()) {
                    Text(hint, color = Color(0xFFFF6B7A), fontSize = 11.sp)
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("取消", color = TextDim) }
                    Spacer(Modifier.width(4.dp))
                    Button(onClick = {
                        val n = days.toIntOrNull() ?: 0
                        val first = try { LocalDate.parse(startDate.trim()) } catch (_: Exception) { null }
                        if (title.isBlank() || first == null || n < 1 || n > 30 || time1.length < 5 || time2.length < 5) {
                            hint = "请填写标题、日期、天数（1-30）和时间"
                        } else {
                            val out = mutableListOf<CtEvent>()
                            var d = first!!
                            var guard = 0
                            while (out.size < n && guard++ < 60) {
                                val weekend = d.dayOfWeek == DayOfWeek.SATURDAY || d.dayOfWeek == DayOfWeek.SUNDAY
                                if (!(skipWeekend && weekend)) {
                                    out.add(
                                        CtEvent(
                                            id = UUID.randomUUID().toString(),
                                            title = title.trim(),
                                            start = "${d}T$time1",
                                            end = "${d}T$time2"
                                        )
                                    )
                                }
                                d = d.plusDays(1)
                            }
                            if (out.isEmpty()) hint = "没有可生成的日期" else onCreate(out)
                        }
                    }) { Text("生成") }
                }
            }
        }
    }
}

@Composable
private fun TemplateDialog(
    templates: List<CtTemplate>,
    onUse: (CtTemplate) -> Unit,
    onDelete: (CtTemplate) -> Unit,
    onDismiss: () -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 480.dp)) {
                Text("日程模板", color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Text("点模板套用；编辑日程时可以「存为模板」", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                if (templates.isEmpty()) {
                    Text("还没有模板", color = TextDim, modifier = Modifier.padding(top = 12.dp))
                } else {
                    Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState()).padding(top = 8.dp)) {
                        templates.forEach { t ->
                            Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                                Column(Modifier.weight(1f).clickable { onUse(t) }) {
                                    Text(t.title, color = TextMain, fontSize = 14.sp)
                                    Text("${t.startTime} – ${t.endTime}" + if (t.location.isNotEmpty()) " · ${t.location}" else "", color = TextDim, fontSize = 11.sp)
                                }
                                TextButton(onClick = { onDelete(t) }) { Text("删除", color = TextDim, fontSize = 11.sp) }
                            }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

@Composable
private fun EditDialog(
    event: CtEvent,
    allTags: List<CtTag> = emptyList(),
    existing: List<CtEvent> = emptyList(),
    onSaveTemplate: ((CtTemplate) -> Unit)? = null,
    onDelete: (() -> Unit)? = null,
    onDismiss: () -> Unit,
    onSave: (CtEvent) -> Unit
) {
    val ctx = LocalContext.current
    var title by remember { mutableStateOf(event.title) }
    var date by remember { mutableStateOf(if (event.start.length >= 10) event.start.substring(0, 10) else LocalDate.now().toString()) }
    var endDate by remember {
        mutableStateOf(
            if (event.end.length >= 10) event.end.substring(0, 10)
            else if (event.start.length >= 10) event.start.substring(0, 10)
            else LocalDate.now().toString()
        )
    }
    var time1 by remember { mutableStateOf(if (event.start.length >= 16) event.start.substring(11, 16) else "09:00") }
    var time2 by remember { mutableStateOf(if (event.end.length >= 16) event.end.substring(11, 16) else "10:00") }
    var location by remember { mutableStateOf(event.location) }
    var priority by remember { mutableStateOf(event.priority) }
    var color by remember { mutableStateOf(event.color) }
    var selectedTags by remember { mutableStateOf(event.tags.toMutableList()) }
    var alarmsSel by remember { mutableStateOf(event.alarms.toMutableList()) }
    var alarmCustom by remember { mutableStateOf("") }
    var recurSel by remember { mutableStateOf(RecurFreq(event)) }
    var recurEnd by remember { mutableStateOf(if (event.recurJson.contains("UNTIL")) 1 else if (event.recurJson.contains("COUNT")) 2 else 0) }
    var recurUntil by remember {
        mutableStateOf(
            Regex("UNTIL=([0-9]{4})([0-9]{2})([0-9]{2})").find(event.recurJson)
                ?.let { "${it.groupValues[1]}-${it.groupValues[2]}-${it.groupValues[3]}" } ?: ""
        )
    }
    var recurCount by remember {
        mutableStateOf(Regex("COUNT=([0-9]+)").find(event.recurJson)?.groupValues?.get(1) ?: "")
    }
    var exDates by remember {
        mutableStateOf(
            Regex("EXDATE=([^;]*)").find(event.recurJson)?.groupValues?.get(1)?.replace(",", ", ") ?: ""
        )
    }
    var ddl by remember { mutableStateOf(event.ddl) }
    var done by remember { mutableStateOf(event.done) }
    var allDay by remember { mutableStateOf(event.allDay) }
    var url by remember { mutableStateOf(event.url) }
    var desc by remember { mutableStateOf(event.desc) }
    var statusSel by remember { mutableStateOf(event.status) }
    var alarmSound by remember { mutableStateOf(event.alarmSound) }
    var alarmText by remember { mutableStateOf(event.alarmText) }
    val baseRec = remember { event.recur() }
    var byDaySel by remember {
        mutableStateOf(
            (if (baseRec.byDay.isEmpty())
                listOf(runCatching { LocalDate.parse(date).dayOfWeek.value % 7 }.getOrDefault(1))
            else baseRec.byDay).toMutableList()
        )
    }
    var intervalStr by remember { mutableStateOf(baseRec.interval.coerceAtLeast(1).toString()) }
    var lunarRecur by remember { mutableStateOf(baseRec.lunar) }

    val palette = listOf(
        "", "#6EA8FF", "#FFB454", "#FF6B7A", "#A3E635", "#C084FC", "#F472B6",
        "#4FD1C5", "#F9A8D4", "#8E7BFF", "#FFD166", "#94A3B8"
    )
    var customColor by remember { mutableStateOf(if (event.color.isNotEmpty() && !palette.contains(event.color)) event.color else "") }

    Dialog(onDismissRequest = onDismiss) {
        Card(colors = CardDefaults.cardColors(containerColor = CardColor)) {
            Column(Modifier.padding(16.dp).heightIn(max = 560.dp)) {
                // 可滚动字段区
                Column(Modifier.weight(1f, fill = false).verticalScroll(rememberScrollState())) {
                    Text(
                        if (event.title.isEmpty()) "新建日程" else "编辑日程",
                        color = TextMain, fontWeight = FontWeight.Bold, fontSize = 18.sp
                    )
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        var smartTxt by remember { mutableStateOf("") }
                        OutlinedTextField(
                            value = smartTxt, onValueChange = { smartTxt = it },
                            label = { Text("智能输入：明天9点开会 #课程") },
                            singleLine = true,
                            modifier = Modifier.weight(1f)
                        )
                        Spacer(Modifier.width(6.dp))
                        Button(onClick = {
                            val p2 = SmartParse.parse(smartTxt)
                            if (p2.title.isNotEmpty()) title = p2.title
                            p2.date?.let { date = it.toString(); endDate = it.toString() }
                            p2.time?.let { t ->
                                time1 = t
                                time2 = SmartParse.addMinutes(t, 60)
                            }
                            if (p2.tagNames.isNotEmpty()) {
                                val ids = mutableListOf<String>()
                                for (n in p2.tagNames) {
                                    val tag = allTags.firstOrNull { it.name == n } ?: allTags.firstOrNull { it.name.contains(n) }
                                    if (tag != null) ids.add(tag.id)
                                }
                                if (ids.isNotEmpty()) selectedTags = ids.toMutableList()
                            }
                            android.widget.Toast.makeText(
                                ctx,
                                "已解析：" + (p2.title.ifEmpty { "未识别标题" }) +
                                    (p2.date?.let { " · $it" } ?: "") + (p2.time?.let { " · $it" } ?: ""),
                                android.widget.Toast.LENGTH_SHORT
                            ).show()
                        }) { Text("填充") }
                    }
                    OutlinedTextField(value = title, onValueChange = { title = it }, label = { Text("标题") }, modifier = Modifier.fillMaxWidth())
                    Row(Modifier.fillMaxWidth()) {
                        OutlinedTextField(
                            value = date, onValueChange = { date = it; if (endDate < date) endDate = date },
                            label = { Text("开始日期") }, singleLine = true,
                            modifier = Modifier.weight(1f)
                        )
                        Spacer(Modifier.width(8.dp))
                        OutlinedTextField(
                            value = endDate, onValueChange = { endDate = it },
                            label = { Text("结束日期") }, singleLine = true,
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = allDay, onCheckedChange = { allDay = it })
                        Text("全天事件", color = TextMain, fontSize = 13.sp)
                    }
                    if (!allDay) {
                        Row(Modifier.fillMaxWidth()) {
                            OutlinedTextField(value = time1, onValueChange = { time1 = it }, label = { Text("开始") }, modifier = Modifier.weight(1f))
                            Spacer(Modifier.width(8.dp))
                            OutlinedTextField(value = time2, onValueChange = { time2 = it }, label = { Text("结束") }, modifier = Modifier.weight(1f))
                        }
                    }
                    OutlinedTextField(value = location, onValueChange = { location = it }, label = { Text("地点") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = url, onValueChange = { url = it }, label = { Text("链接 URL（可空）") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = desc, onValueChange = { desc = it }, label = { Text("描述（可空）") }, minLines = 2, maxLines = 4, modifier = Modifier.fillMaxWidth())
                    Text("优先级", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf("无", "低", "中", "高").forEachIndexed { i, label ->
                            Box(
                                Modifier
                                    .background(if (priority == i) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                    .clickable { priority = i }
                                    .padding(horizontal = 10.dp, vertical = 4.dp)
                            ) {
                                Text(label, color = if (priority == i) Accent else TextDim, fontSize = 12.sp)
                            }
                        }
                    }
                    Text("状态", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf(0 to "已确认", 1 to "暂定", 2 to "已取消").forEach { (v, label) ->
                            Box(
                                Modifier
                                    .background(if (statusSel == v) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                    .clickable { statusSel = v }
                                    .padding(horizontal = 10.dp, vertical = 4.dp)
                            ) {
                                Text(label, color = if (statusSel == v) Accent else TextDim, fontSize = 12.sp)
                            }
                        }
                    }
                    Text("提醒（可多选）", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf(0, 5, 10, 15, 30, 60, 120).forEach { min ->
                            val on = alarmsSel.contains(min)
                            Box(
                                Modifier
                                    .background(if (on) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                    .clickable {
                                        alarmsSel = if (on) {
                                            alarmsSel.filter { it != min }.toMutableList()
                                        } else {
                                            (alarmsSel + min).distinct().sorted().toMutableList()
                                        }
                                    }
                                    .padding(horizontal = 8.dp, vertical = 4.dp)
                            ) {
                                Text(if (min == 0) "准时" else "${min}分", color = if (on) Accent else TextDim, fontSize = 12.sp)
                            }
                        }
                        if (alarmsSel.isEmpty()) {
                            Box(Modifier.padding(horizontal = 8.dp, vertical = 4.dp)) {
                                Text("无提醒", color = TextDim, fontSize = 12.sp)
                            }
                        }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = alarmSound, onCheckedChange = { alarmSound = it })
                        Text("响铃", color = TextMain, fontSize = 13.sp)
                        Spacer(Modifier.width(6.dp))
                        OutlinedTextField(
                            value = alarmText, onValueChange = { alarmText = it },
                            label = { Text("提醒文本（可空）") }, singleLine = true,
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        OutlinedTextField(
                            value = alarmCustom, onValueChange = { alarmCustom = it },
                            label = { Text("自定义分钟") }, singleLine = true,
                            modifier = Modifier.weight(1f)
                        )
                        TextButton(onClick = {
                            val v = alarmCustom.toIntOrNull()
                            if (v != null && v in 0..10080) {
                                alarmsSel = (alarmsSel + v).distinct().sorted().toMutableList()
                                alarmCustom = ""
                            }
                        }) { Text("添加", color = Accent) }
                    }
                    Text("重复", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf(0 to "不重复", 1 to "每天", 2 to "每周", 3 to "每月", 4 to "每年", 5 to "工作日").forEach { (freq, label) ->
                            Box(
                                Modifier
                                    .background(if (recurSel == freq) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                    .clickable { recurSel = freq; if (freq == 0) recurEnd = 0 }
                                    .padding(horizontal = 8.dp, vertical = 4.dp)
                            ) {
                                Text(label, color = if (recurSel == freq) Accent else TextDim, fontSize = 12.sp)
                            }
                        }
                    }
                    if (recurSel != 0) {
                        Text("结束条件", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 6.dp))
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            listOf(0 to "永不", 1 to "日期", 2 to "次数").forEach { (mode, label) ->
                                Box(
                                    Modifier
                                        .background(if (recurEnd == mode) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                        .clickable { recurEnd = mode }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(label, color = if (recurEnd == mode) Accent else TextDim, fontSize = 12.sp)
                                }
                            }
                        }
                        if (recurEnd == 1) {
                            OutlinedTextField(value = recurUntil, onValueChange = { recurUntil = it }, label = { Text("结束日期 YYYY-MM-DD") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                        }
                        if (recurEnd == 2) {
                            OutlinedTextField(value = recurCount, onValueChange = { recurCount = it }, label = { Text("重复次数") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                        }
                        OutlinedTextField(value = exDates, onValueChange = { exDates = it }, label = { Text("例外日 YYYY-MM-DD, 逗号分隔（可空）") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                        if (recurSel == 2 || recurSel == 5) {
                            Text("重复星期", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 6.dp))
                            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                listOf(1 to "一", 2 to "二", 3 to "三", 4 to "四", 5 to "五", 6 to "六", 0 to "日").forEach { (d, label) ->
                                    val on = byDaySel.contains(d)
                                    Box(
                                        Modifier
                                            .background(if (on) Accent.copy(alpha = 0.25f) else Color.Transparent, RoundedCornerShape(6.dp))
                                            .clickable {
                                                byDaySel = if (on) {
                                                    byDaySel.filter { it != d }.toMutableList()
                                                } else {
                                                    (byDaySel + d).sorted().toMutableList()
                                                }
                                            }
                                            .padding(horizontal = 10.dp, vertical = 4.dp)
                                    ) {
                                        Text(label, color = if (on) Accent else TextDim, fontSize = 12.sp)
                                    }
                                }
                            }
                        }
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("间隔", color = TextDim, fontSize = 12.sp)
                            Spacer(Modifier.width(6.dp))
                            OutlinedTextField(
                                value = intervalStr, onValueChange = { intervalStr = it },
                                singleLine = true, modifier = Modifier.width(80.dp)
                            )
                            Spacer(Modifier.width(6.dp))
                            Text(
                                when (recurSel) { 1 -> "天"; 2 -> "周"; 3 -> "月"; 4 -> "年"; else -> "期" },
                                color = TextDim, fontSize = 12.sp
                            )
                        }
                        if (recurSel == 4) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Checkbox(checked = lunarRecur, onCheckedChange = { lunarRecur = it })
                                Text("按农历日期重复", color = TextMain, fontSize = 13.sp)
                            }
                        }
                    }
                    Text("颜色", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                    OutlinedTextField(
                        value = customColor,
                        onValueChange = { v ->
                            customColor = v
                            val t = v.trim()
                            if (t.matches(Regex("^#[0-9a-fA-F]{6}$"))) color = t
                        },
                        label = { Text("自定义颜色 #RRGGBB（可空）") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth().padding(top = 4.dp)
                    )
                    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    palette.chunked(6).forEach { rowColors ->
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        rowColors.forEach { hex ->
                            val c = if (hex.isEmpty()) TextDim else EvColor(CtEvent(color = hex))
                            Box(
                                Modifier.size(26.dp).background(c, RoundedCornerShape(50))
                                    .then(if (color == hex) Modifier.border(2.dp, TextMain, RoundedCornerShape(50)) else Modifier)
                                    .clickable { color = hex }
                            )
                        }
                    }
                    }
                    }
                    }
                    if (allTags.isNotEmpty()) {
                        Text("标签", color = TextDim, fontSize = 11.sp, modifier = Modifier.padding(top = 8.dp))
                        Row(
                            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            allTags.forEach { tag ->
                                val tc = try {
                                    if (tag.color.length == 7 && tag.color.startsWith("#")) Color(android.graphics.Color.parseColor(tag.color)) else Accent
                                } catch (_: Exception) {
                                    Accent
                                }
                                val on = selectedTags.contains(tag.id)
                                Box(
                                    Modifier
                                        .background(if (on) tc.copy(alpha = 0.35f) else Color.Transparent, RoundedCornerShape(6.dp))
                                        .then(if (on) Modifier.border(1.dp, tc, RoundedCornerShape(6.dp)) else Modifier)
                                        .clickable {
                                            selectedTags = if (on) {
                                                selectedTags.filter { it != tag.id }.toMutableList()
                                            } else {
                                                (selectedTags + tag.id).toMutableList()
                                            }
                                        }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(tag.name, color = if (on) tc else TextDim, fontSize = 11.sp)
                                }
                            }
                        }
                    }
                    val conflict = existing.firstOrNull { other ->
                        other.id != event.id && other.start.startsWith(date) &&
                            other.start.length >= 16 && other.end.length >= 16 &&
                            other.start.substring(11, 16) < time2 && other.end.substring(11, 16) > time1
                    }
                    if (conflict != null) {
                        Text(
                            "⚠ 与「${conflict.title.ifEmpty { "未命名" }}」时间重叠",
                            color = Color(0xFFFF6B7A), fontSize = 11.sp,
                            modifier = Modifier.padding(top = 4.dp)
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = ddl, onCheckedChange = { ddl = it })
                        Text("设为 DDL", color = TextMain)
                        Spacer(Modifier.width(12.dp))
                        Checkbox(checked = done, onCheckedChange = { done = it })
                        Text("已完成", color = TextMain)
                    }
                }
                // 固定底部按钮栏
                Row(
                    Modifier.fillMaxWidth().padding(top = 8.dp),
                    horizontalArrangement = Arrangement.End
                ) {
                    if (onDelete != null) {
                        TextButton(onClick = onDelete) { Text("删除", color = Color(0xFFFF6B7A)) }
                        Spacer(Modifier.width(4.dp))
                    }
                    if (onSaveTemplate != null) {
                        TextButton(onClick = {
                            onSaveTemplate(
                                CtTemplate(
                                    title = title.ifEmpty { "未命名" },
                                    location = location,
                                    startTime = time1,
                                    endTime = time2,
                                    priority = priority,
                                    color = color,
                                    alarm = alarmsSel.firstOrNull() ?: -1,
                                    recurFreq = recurSel
                                )
                            )
                        }) { Text("存为模板", color = TextDim) }
                        Spacer(Modifier.width(4.dp))
                    }
                    TextButton(onClick = onDismiss) { Text("取消", color = TextDim) }
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = {
                        val start = "${date}T$time1"
                        val end = "${date}T$time2"
                        onSave(
                            event.copy(
                                title = title.ifEmpty { "未命名" },
                                start = if (allDay) "${date}T00:00" else start,
                                end = if (allDay) "${endDate}T23:59" else "${endDate}T$time2",
                                allDay = allDay,
                                location = location,
                                url = url,
                                desc = desc,
                                priority = priority,
                                status = statusSel,
                                color = color,
                                tags = selectedTags,
                                alarms = alarmsSel.sorted().toMutableList(),
                                alarmSound = alarmSound,
                                alarmText = alarmText,
                                recurJson = run {
                                    if (recurSel == 0) return@run ""
                                    val base = event.recur()
                                    var r = base.copy(
                                        freq = when (recurSel) {
                                            1 -> "daily"
                                            2 -> "weekly"
                                            3 -> "monthly"
                                            4 -> "yearly"
                                            5 -> "weekly"
                                            else -> "none"
                                        },
                                        interval = intervalStr.toIntOrNull()?.coerceAtLeast(1)
                                            ?: base.interval.coerceAtLeast(1)
                                    )
                                    r = when (recurSel) {
                                        5 -> r.copy(byDay = listOf(1, 2, 3, 4, 5))
                                        2 -> r.copy(byDay = if (byDaySel.isEmpty()) listOf(1) else byDaySel.sorted())
                                        else -> r
                                    }
                                    if (recurSel == 4) r = r.copy(lunar = lunarRecur)
                                    r = when (recurEnd) {
                                        1 -> {
                                            val u = recurUntil.trim()
                                            if (u.length >= 10) r.copy(until = "${u.take(10)}T23:59", count = 0) else r
                                        }
                                        2 -> {
                                            val c = recurCount.toIntOrNull() ?: 0
                                            r.copy(count = c, until = "")
                                        }
                                        else -> r.copy(until = "", count = 0)
                                    }
                                    val ex = exDates.split(",", "，", " ")
                                        .map { it.trim() }
                                        .filter { it.matches(Regex("^[0-9]{4}-[0-9]{2}-[0-9]{2}$")) }
                                    r = r.copy(exDates = ex)
                                    r.toJson().toString()
                                },
                                ddl = ddl,
                                done = done
                            )
                        )
                    }) { Text("保存") }
                }
            }
        }
    }
