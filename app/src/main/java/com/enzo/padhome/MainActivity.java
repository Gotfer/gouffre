package com.enzo.padhome;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.TextClock;
import android.widget.TextView;
import android.widget.Toast;

import java.text.Collator;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** PadHome : écran d'accueil façon iPadOS pour Galaxy Tab. */
public class MainActivity extends Activity implements Pager.Listener {

    static class App {
        String label;
        ComponentName cn;
        Bitmap icon;
        String key() { return cn.flattenToShortString(); }
    }

    private static final int MAX_DOCK = 8;
    private static final String[] DEFAULT_DOCK = {
            "com.android.chrome", "com.sec.android.app.sbrowser",
            "com.google.android.youtube", "com.google.android.gm",
            "com.google.android.apps.photos", "com.sec.android.gallery3d",
            "com.sec.android.app.camera", "com.android.settings"
    };

    private final List<App> apps = new ArrayList<>();
    private final Map<String, App> byKey = new HashMap<>();
    private SharedPreferences prefs;
    private float dp;
    private int iconPx;

    private LinearLayout content, widgets, dots, dock;
    private Pager pager;
    private FrameLayout searchLayer;
    private EditText searchBox;
    private GridLayout searchGrid;
    private Ring batteryRing;
    private TextView batteryText;
    private int lastW, lastH;

    // ---------------------------------------------------------------- cycle de vie

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        prefs = getSharedPreferences("padhome", MODE_PRIVATE);
        dp = getResources().getDisplayMetrics().density;
        boolean tablet = getResources().getConfiguration().smallestScreenWidthDp >= 600;
        iconPx = px(tablet ? 72 : 58);

        Window w = getWindow();
        w.setStatusBarColor(Color.TRANSPARENT);
        w.setNavigationBarColor(Color.TRANSPARENT);
        if (Build.VERSION.SDK_INT >= 29) w.setNavigationBarContrastEnforced(false);
        if (Build.VERSION.SDK_INT >= 30) w.setDecorFitsSystemWindows(false);
        else w.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        setContentView(buildUi());
        loadApps();

        IntentFilter pf = new IntentFilter();
        pf.addAction(Intent.ACTION_PACKAGE_ADDED);
        pf.addAction(Intent.ACTION_PACKAGE_REMOVED);
        pf.addAction(Intent.ACTION_PACKAGE_CHANGED);
        pf.addDataScheme("package");
        register(packageReceiver, pf);
        register(batteryReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
    }

    private void register(BroadcastReceiver r, IntentFilter f) {
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(r, f, Context.RECEIVER_EXPORTED);
        else registerReceiver(r, f);
    }

    @Override protected void onDestroy() {
        try { unregisterReceiver(packageReceiver); unregisterReceiver(batteryReceiver); } catch (Exception ignored) {}
        super.onDestroy();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // Bouton Accueil pressé alors qu'on est déjà sur l'accueil : retour à la première page.
        if (searchLayer.getVisibility() == View.VISIBLE) closeSearch();
        else pager.snapTo(0);
    }

    @Override public void onBackPressed() {
        if (searchLayer.getVisibility() == View.VISIBLE) closeSearch();
        // Sinon on ne fait rien : c'est l'écran d'accueil.
    }

    private final BroadcastReceiver packageReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) { loadApps(); }
    };

    private final BroadcastReceiver batteryReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            int level = i.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = i.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
            boolean charging = i.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) != 0;
            if (level < 0 || scale <= 0) return;
            int pct = Math.round(level * 100f / scale);
            batteryRing.set(pct / 100f, charging);
            batteryText.setText(pct + " %" + (charging ? " ⚡" : ""));
        }
    };

    // ---------------------------------------------------------------- interface

    private View buildUi() {
        FrameLayout root = new FrameLayout(this);

        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        root.addView(content, new FrameLayout.LayoutParams(-1, -1));

        // Rangée de widgets (horloge + batterie), comme sur iPad.
        widgets = new LinearLayout(this);
        widgets.setOrientation(LinearLayout.HORIZONTAL);
        widgets.setPadding(px(32), px(20), px(32), px(4));
        widgets.addView(clockWidget(), new LinearLayout.LayoutParams(px(330), px(158)));
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(px(158), px(158));
        blp.leftMargin = px(22);
        widgets.addView(batteryWidget(), blp);
        content.addView(widgets, new LinearLayout.LayoutParams(-1, -2));

        pager = new Pager(this);
        pager.setListener(this);
        content.addView(pager, new LinearLayout.LayoutParams(-1, 0, 1f));
        pager.addOnLayoutChangeListener((v, l, t, r, bt, ol, ot, or, ob) -> {
            if (r - l != lastW || bt - t != lastH) {
                lastW = r - l; lastH = bt - t;
                v.post(this::rebuildPages);
            }
        });

        dots = new LinearLayout(this);
        dots.setGravity(Gravity.CENTER);
        content.addView(dots, new LinearLayout.LayoutParams(-1, px(26)));

        dock = new LinearLayout(this);
        dock.setOrientation(LinearLayout.HORIZONTAL);
        dock.setGravity(Gravity.CENTER);
        dock.setPadding(px(14), px(12), px(14), px(12));
        dock.setBackground(rounded(0x40FFFFFF, px(30), 0x26FFFFFF));
        dock.setMinimumWidth(px(120));
        dock.setMinimumHeight(iconPx + px(24));
        LinearLayout.LayoutParams dlp = new LinearLayout.LayoutParams(-2, -2);
        dlp.gravity = Gravity.CENTER_HORIZONTAL;
        dlp.bottomMargin = px(14);
        content.addView(dock, dlp);

        root.addView(buildSearchLayer(), new FrameLayout.LayoutParams(-1, -1));

        root.setOnApplyWindowInsetsListener((v, insets) -> {
            content.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            searchLayer.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        return root;
    }

    private View clockWidget() {
        LinearLayout card = card();
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(px(22), px(14), px(22), px(14));

        TextClock day = new TextClock(this);
        day.setFormat12Hour("EEEE d MMMM");
        day.setFormat24Hour("EEEE d MMMM");
        day.setTextColor(0xFFFF453A); // rouge Apple
        day.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        day.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        day.setAllCaps(true);
        card.addView(day);

        TextClock time = new TextClock(this);
        time.setFormat12Hour("HH:mm");
        time.setFormat24Hour("HH:mm");
        time.setTextColor(Color.WHITE);
        time.setTextSize(TypedValue.COMPLEX_UNIT_SP, 64);
        time.setTypeface(Typeface.create("sans-serif-thin", Typeface.NORMAL));
        time.setIncludeFontPadding(false);
        card.addView(time);
        card.setOnClickListener(v -> tryStart(new Intent("android.intent.action.SHOW_ALARMS")));
        return card;
    }

    private View batteryWidget() {
        LinearLayout card = card();
        card.setGravity(Gravity.CENTER);
        FrameLayout ringBox = new FrameLayout(this);
        batteryRing = new Ring(this);
        ringBox.addView(batteryRing, new FrameLayout.LayoutParams(px(84), px(84), Gravity.CENTER));
        ImageView tab = new ImageView(this);
        tab.setImageResource(android.R.drawable.ic_lock_idle_charging);
        tab.setColorFilter(Color.WHITE);
        ringBox.addView(tab, new FrameLayout.LayoutParams(px(30), px(30), Gravity.CENTER));
        card.addView(ringBox, new LinearLayout.LayoutParams(px(90), px(90)));
        batteryText = label("— %", 15, Color.WHITE);
        batteryText.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        batteryText.setPadding(0, px(8), 0, 0);
        card.addView(batteryText);
        card.setOnClickListener(v -> tryStart(new Intent(Intent.ACTION_POWER_USAGE_SUMMARY)));
        return card;
    }

    private LinearLayout card() {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setBackground(rounded(0x591C1C1E, px(24), 0x1FFFFFFF));
        c.setElevation(px(2));
        return c;
    }

    private View buildSearchLayer() {
        searchLayer = new FrameLayout(this);
        searchLayer.setBackgroundColor(0xC8101014);
        searchLayer.setVisibility(View.GONE);
        searchLayer.setOnClickListener(v -> closeSearch());

        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(px(60), px(36), px(60), 0);
        searchLayer.addView(col, new FrameLayout.LayoutParams(-1, -1));

        searchBox = new EditText(this);
        searchBox.setHint("Rechercher");
        searchBox.setHintTextColor(0x99FFFFFF);
        searchBox.setTextColor(Color.WHITE);
        searchBox.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
        searchBox.setSingleLine(true);
        searchBox.setImeOptions(EditorInfo.IME_ACTION_SEARCH);
        searchBox.setBackground(rounded(0x33FFFFFF, px(16), 0));
        searchBox.setPadding(px(18), px(12), px(18), px(12));
        Drawable mag = getDrawable(android.R.drawable.ic_menu_search);
        if (mag != null) { mag.setTint(0xCCFFFFFF); mag.setBounds(0, 0, px(24), px(24)); }
        searchBox.setCompoundDrawables(mag, null, null, null);
        searchBox.setCompoundDrawablePadding(px(10));
        LinearLayout.LayoutParams slp = new LinearLayout.LayoutParams(-1, -2);
        slp.gravity = Gravity.CENTER_HORIZONTAL;
        col.addView(searchBox, slp);

        TextView title = label("Bibliothèque d'apps", 13, 0x99FFFFFF);
        title.setGravity(Gravity.START);
        title.setPadding(px(4), px(22), 0, px(10));
        title.setAllCaps(true);
        col.addView(title, new LinearLayout.LayoutParams(-1, -2));

        ScrollView sv = new ScrollView(this);
        sv.setVerticalScrollBarEnabled(false);
        searchGrid = new GridLayout(this);
        sv.addView(searchGrid, new FrameLayout.LayoutParams(-1, -2));
        col.addView(sv, new LinearLayout.LayoutParams(-1, 0, 1f));

        searchBox.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            public void onTextChanged(CharSequence s, int a, int b, int c) {}
            public void afterTextChanged(Editable e) { fillSearch(e.toString(), title); }
        });
        searchBox.setOnEditorActionListener((v, actionId, ev) -> {
            boolean enter = actionId == EditorInfo.IME_ACTION_SEARCH
                    || (ev != null && ev.getKeyCode() == KeyEvent.KEYCODE_ENTER && ev.getAction() == KeyEvent.ACTION_UP);
            if (!enter) return false;
            List<App> res = filter(searchBox.getText().toString());
            if (!res.isEmpty()) launch(res.get(0), null);
            else {
                Intent web = new Intent(Intent.ACTION_WEB_SEARCH);
                web.putExtra("query", searchBox.getText().toString());
                tryStart(web);
            }
            closeSearch();
            return true;
        });
        return searchLayer;
    }

    // ---------------------------------------------------------------- applications

    private void loadApps() {
        final int size = iconPx;
        new Thread(() -> {
            PackageManager pm = getPackageManager();
            Intent q = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
            List<ResolveInfo> ris = pm.queryIntentActivities(q, 0);
            List<App> list = new ArrayList<>();
            for (ResolveInfo ri : ris) {
                if (ri.activityInfo.packageName.equals(getPackageName())) continue;
                App a = new App();
                a.label = ri.loadLabel(pm).toString();
                a.cn = new ComponentName(ri.activityInfo.packageName, ri.activityInfo.name);
                Drawable d = null;
                int res = ri.getIconResource();
                if (res != 0) {
                    try {
                        Resources r = pm.getResourcesForApplication(ri.activityInfo.applicationInfo);
                        d = r.getDrawableForDensity(res, DisplayMetrics.DENSITY_XXXHIGH, null);
                    } catch (Exception ignored) {}
                }
                if (d == null) d = ri.loadIcon(pm);
                try { a.icon = IconUtil.make(d, size); } catch (Exception e) { continue; }
                list.add(a);
            }
            Collator col = Collator.getInstance(Locale.FRENCH);
            list.sort((x, y) -> col.compare(x.label, y.label));
            runOnUiThread(() -> {
                apps.clear(); apps.addAll(list);
                byKey.clear();
                for (App a : apps) byKey.put(a.key(), a);
                ensureDefaultDock();
                rebuildDock();
                rebuildPages();
            });
        }).start();
    }

    private List<String> dockKeys() {
        String s = prefs.getString("dock", null);
        List<String> out = new ArrayList<>();
        if (s == null || s.isEmpty()) return out;
        out.addAll(Arrays.asList(s.split("\\|")));
        return out;
    }

    private void saveDock(List<String> keys) { prefs.edit().putString("dock", TextUtils.join("|", keys)).apply(); }

    private Set<String> hidden() { return new HashSet<>(prefs.getStringSet("hidden", new HashSet<>())); }

    private void ensureDefaultDock() {
        if (prefs.contains("dock")) return;
        List<String> keys = new ArrayList<>();
        for (String pkg : DEFAULT_DOCK) {
            if (keys.size() >= 5) break;
            for (App a : apps) {
                if (a.cn.getPackageName().equals(pkg)) { keys.add(a.key()); break; }
            }
        }
        saveDock(keys);
    }

    private void rebuildDock() {
        dock.removeAllViews();
        for (String k : dockKeys()) {
            App a = byKey.get(k);
            if (a == null) continue;
            View v = iconView(a, false, 0);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, -2);
            lp.leftMargin = lp.rightMargin = px(9);
            dock.addView(v, lp);
        }
    }

    private void rebuildPages() {
        int w = pager.getWidth(), h = pager.getHeight();
        if (w == 0 || h == 0 || apps.isEmpty()) return;

        // En paysage peu haut, on masque les widgets pour garder de la place aux icônes.
        boolean roomy = h + widgets.getHeight() > iconPx * 5;
        widgets.setVisibility(roomy ? View.VISIBLE : View.GONE);

        int cellW = iconPx + px(52), cellH = iconPx + px(46);
        int cols = clamp(w / cellW, 4, 8), rows = clamp(h / cellH, 2, 7);
        int per = cols * rows;

        Set<String> skip = new HashSet<>(dockKeys());
        skip.addAll(hidden());
        List<App> list = new ArrayList<>();
        for (App a : apps) if (!skip.contains(a.key())) list.add(a);

        int count = Math.max(1, (list.size() + per - 1) / per);
        View[] pages = new View[count];
        int gapX = w / cols, gapY = h / rows;
        for (int p = 0; p < count; p++) {
            FrameLayout page = new FrameLayout(this);
            for (int i = 0; i < per; i++) {
                int idx = p * per + i;
                if (idx >= list.size()) break;
                View v = iconView(list.get(idx), true, cellW);
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(cellW, cellH);
                lp.leftMargin = (i % cols) * gapX + (gapX - cellW) / 2;
                lp.topMargin = (i / cols) * gapY + (gapY - cellH) / 2;
                page.addView(v, lp);
            }
            pages[p] = page;
        }
        int keep = pager.getPage();
        pager.setPages(pages);
        pager.setPageImmediate(Math.min(keep, count - 1));
    }

    private View iconView(App a, boolean withLabel, int width) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER_HORIZONTAL);

        ImageView img = new ImageView(this);
        img.setImageBitmap(a.icon);
        img.setElevation(px(3));
        final float radius = iconPx * 0.23f;
        img.setOutlineProvider(new ViewOutlineProvider() {
            @Override public void getOutline(View v, Outline o) {
                o.setRoundRect(0, 0, v.getWidth(), v.getHeight(), radius);
            }
        });
        box.addView(img, new LinearLayout.LayoutParams(iconPx, iconPx));

        if (withLabel) {
            TextView t = label(a.label, 12, Color.WHITE);
            t.setSingleLine(true);
            t.setEllipsize(TextUtils.TruncateAt.END);
            t.setShadowLayer(px(3), 0, px(1), 0x99000000);
            t.setPadding(px(2), px(6), px(2), 0);
            box.addView(t, new LinearLayout.LayoutParams(width > 0 ? width : -2, -2));
        }

        box.setOnTouchListener((v, e) -> {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN)
                img.animate().scaleX(0.88f).scaleY(0.88f).alpha(0.8f).setDuration(110).start();
            else if (e.getActionMasked() == MotionEvent.ACTION_UP || e.getActionMasked() == MotionEvent.ACTION_CANCEL)
                img.animate().scaleX(1f).scaleY(1f).alpha(1f).setDuration(160).start();
            return false;
        });
        box.setOnClickListener(v -> launch(a, img));
        box.setOnLongClickListener(v -> { showAppMenu(a, v); return true; });
        return box;
    }

    private void launch(App a, View from) {
        Intent i = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).setComponent(a.cn)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
        try {
            Bundle opts = from != null
                    ? ActivityOptions.makeScaleUpAnimation(from, 0, 0, from.getWidth(), from.getHeight()).toBundle()
                    : null;
            startActivity(i, opts);
        } catch (Exception e) {
            Toast.makeText(this, "Impossible d'ouvrir " + a.label, Toast.LENGTH_SHORT).show();
        }
    }

    private void tryStart(Intent i) {
        try { startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
        catch (Exception e) { Toast.makeText(this, "Action indisponible", Toast.LENGTH_SHORT).show(); }
    }

    // ---------------------------------------------------------------- menus

    private void showAppMenu(App a, View anchor) {
        anchor.performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
        List<String> keys = dockKeys();
        boolean inDock = keys.contains(a.key());
        PopupMenu m = new PopupMenu(this, anchor);
        m.getMenu().add(0, 1, 0, inDock ? "Retirer du Dock" : "Ajouter au Dock");
        m.getMenu().add(0, 2, 1, "Masquer l'app");
        m.getMenu().add(0, 3, 2, "Infos sur l'app");
        m.getMenu().add(0, 4, 3, "Désinstaller");
        m.setOnMenuItemClickListener(item -> {
            switch (item.getItemId()) {
                case 1:
                    if (inDock) keys.remove(a.key());
                    else if (keys.size() >= MAX_DOCK) { Toast.makeText(this, "Le Dock est plein", Toast.LENGTH_SHORT).show(); return true; }
                    else keys.add(a.key());
                    saveDock(keys); rebuildDock(); rebuildPages();
                    return true;
                case 2: {
                    Set<String> h = hidden(); h.add(a.key());
                    keys.remove(a.key()); saveDock(keys);
                    prefs.edit().putStringSet("hidden", h).apply();
                    rebuildDock(); rebuildPages();
                    return true;
                }
                case 3:
                    tryStart(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                            Uri.parse("package:" + a.cn.getPackageName())));
                    return true;
                case 4:
                    tryStart(new Intent(Intent.ACTION_DELETE, Uri.parse("package:" + a.cn.getPackageName())));
                    return true;
            }
            return false;
        });
        m.show();
    }

    @Override public void onLongPressEmpty() {
        pager.performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
        PopupMenu m = new PopupMenu(this, dots, Gravity.CENTER);
        m.getMenu().add(0, 1, 0, "Fond d'écran");
        m.getMenu().add(0, 2, 1, "Apps masquées (" + hidden().size() + ")");
        m.getMenu().add(0, 3, 2, "Choisir l'appli d'accueil");
        m.getMenu().add(0, 4, 3, "Paramètres");
        m.setOnMenuItemClickListener(item -> {
            switch (item.getItemId()) {
                case 1: tryStart(Intent.createChooser(new Intent(Intent.ACTION_SET_WALLPAPER), "Fond d'écran")); return true;
                case 2: showHiddenDialog(); return true;
                case 3: tryStart(new Intent(Settings.ACTION_HOME_SETTINGS)); return true;
                case 4: tryStart(new Intent(Settings.ACTION_SETTINGS)); return true;
            }
            return false;
        });
        m.show();
    }

    private void showHiddenDialog() {
        Set<String> h = hidden();
        List<String> keys = new ArrayList<>();
        List<String> names = new ArrayList<>();
        for (String k : h) { App a = byKey.get(k); if (a != null) { keys.add(k); names.add(a.label); } }
        if (keys.isEmpty()) { Toast.makeText(this, "Aucune app masquée", Toast.LENGTH_SHORT).show(); return; }
        boolean[] checked = new boolean[keys.size()];
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Réafficher des apps")
                .setMultiChoiceItems(names.toArray(new String[0]), checked, (d, i, c) -> checked[i] = c)
                .setPositiveButton("Réafficher", (d, w) -> {
                    for (int i = 0; i < keys.size(); i++) if (checked[i]) h.remove(keys.get(i));
                    prefs.edit().putStringSet("hidden", h).apply();
                    rebuildPages();
                })
                .setNegativeButton("Annuler", null)
                .show();
    }

    // ---------------------------------------------------------------- recherche (glisser vers le bas)

    @Override public void onSwipeDown() { openSearch(); }

    private void openSearch() {
        if (searchLayer.getVisibility() == View.VISIBLE) return;
        searchBox.setText("");
        searchLayer.setAlpha(0f);
        searchLayer.setTranslationY(-px(30));
        searchLayer.setVisibility(View.VISIBLE);
        searchLayer.animate().alpha(1f).translationY(0).setDuration(220).start();
        searchBox.requestFocus();
        searchBox.postDelayed(() -> {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) imm.showSoftInput(searchBox, InputMethodManager.SHOW_IMPLICIT);
        }, 150);
    }

    private void closeSearch() {
        InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(searchBox.getWindowToken(), 0);
        searchLayer.animate().alpha(0f).translationY(-px(30)).setDuration(180)
                .withEndAction(() -> searchLayer.setVisibility(View.GONE)).start();
    }

    private static String norm(String s) {
        String n = Normalizer.normalize(s, Normalizer.Form.NFD).replaceAll("\\p{M}", "");
        return n.toLowerCase(Locale.ROOT);
    }

    private List<App> filter(String q) {
        String nq = norm(q.trim());
        Set<String> h = hidden();
        List<App> starts = new ArrayList<>(), contains = new ArrayList<>();
        for (App a : apps) {
            if (h.contains(a.key()) && nq.isEmpty()) continue;
            String nl = norm(a.label);
            if (nq.isEmpty() || nl.startsWith(nq)) starts.add(a);
            else if (nl.contains(nq)) contains.add(a);
        }
        starts.addAll(contains);
        return starts;
    }

    private void fillSearch(String q, TextView title) {
        title.setText(q.trim().isEmpty() ? "Bibliothèque d'apps" : "Applications");
        searchGrid.removeAllViews();
        int width = searchGrid.getWidth() > 0 ? searchGrid.getWidth() : getResources().getDisplayMetrics().widthPixels - px(120);
        int cellW = iconPx + px(52);
        int cols = clamp(width / cellW, 3, 10);
        searchGrid.setColumnCount(cols);
        for (App a : filter(q)) {
            View v = iconView(a, true, cellW);
            GridLayout.LayoutParams lp = new GridLayout.LayoutParams();
            lp.width = width / cols;
            lp.height = iconPx + px(56);
            searchGrid.addView(v, lp);
        }
    }

    // ---------------------------------------------------------------- indicateur de pages

    @Override public void onPageChanged(int page, int count) {
        dots.removeAllViews();
        if (count <= 1) return;
        for (int i = 0; i < count; i++) {
            View d = new View(this);
            GradientDrawable g = new GradientDrawable();
            g.setShape(GradientDrawable.OVAL);
            g.setColor(i == page ? Color.WHITE : 0x66FFFFFF);
            d.setBackground(g);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(px(7), px(7));
            lp.leftMargin = lp.rightMargin = px(5);
            final int target = i;
            d.setOnClickListener(v -> pager.snapTo(target));
            dots.addView(d, lp);
        }
    }

    // ---------------------------------------------------------------- utilitaires

    private TextView label(String s, int sp, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setGravity(Gravity.CENTER);
        return t;
    }

    private GradientDrawable rounded(int color, int radius, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(radius);
        if (stroke != 0) g.setStroke(Math.max(1, px(1) / 2), stroke);
        return g;
    }

    private int px(float v) { return Math.round(v * dp); }

    private static int clamp(int v, int lo, int hi) { return Math.max(lo, Math.min(hi, v)); }

    /** Anneau de batterie façon widget iPadOS. */
    static class Ring extends View {
        private final Paint track = new Paint(Paint.ANTI_ALIAS_FLAG), bar = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF box = new RectF();
        private float pct = 0f;

        Ring(Context c) {
            super(c);
            float d = c.getResources().getDisplayMetrics().density;
            for (Paint p : new Paint[]{track, bar}) {
                p.setStyle(Paint.Style.STROKE);
                p.setStrokeWidth(7 * d);
                p.setStrokeCap(Paint.Cap.ROUND);
            }
            track.setColor(0x33FFFFFF);
            bar.setColor(0xFF34C759);
        }

        void set(float p, boolean charging) {
            pct = p;
            bar.setColor(p <= 0.2f && !charging ? 0xFFFF453A : 0xFF34C759);
            invalidate();
        }

        @Override protected void onDraw(Canvas c) {
            float s = track.getStrokeWidth() / 2f;
            box.set(s, s, getWidth() - s, getHeight() - s);
            c.drawArc(box, 0, 360, false, track);
            c.drawArc(box, -90, 360 * pct, false, bar);
        }
    }

}
