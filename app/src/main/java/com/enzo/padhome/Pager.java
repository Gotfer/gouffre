package com.enzo.padhome;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.widget.OverScroller;

/** Pages horizontales façon iPadOS : glisser à gauche/droite, glisser vers le bas = recherche. */
public class Pager extends ViewGroup {

    public interface Listener {
        void onPageChanged(int page, int count);
        void onSwipeDown();
        void onLongPressEmpty();
    }

    private final OverScroller scroller;
    private final int touchSlop, minFling;
    private final GestureDetector gestures;
    private VelocityTracker vt;
    private float downX, downY, lastX;
    private boolean dragging, swipedDown;
    private int page = 0;
    private Listener listener;

    public Pager(Context c) {
        super(c);
        scroller = new OverScroller(c, new DecelerateInterpolator(2.2f));
        ViewConfiguration vc = ViewConfiguration.get(c);
        touchSlop = vc.getScaledTouchSlop();
        minFling = vc.getScaledMinimumFlingVelocity() * 4;
        gestures = new GestureDetector(c, new GestureDetector.SimpleOnGestureListener() {
            @Override public void onLongPress(MotionEvent e) {
                if (!dragging && listener != null) listener.onLongPressEmpty();
            }
        });
    }

    public void setListener(Listener l) { listener = l; }
    public int getPage() { return page; }
    public int getPageCount() { return getChildCount(); }

    @Override protected void onMeasure(int wSpec, int hSpec) {
        int w = MeasureSpec.getSize(wSpec), h = MeasureSpec.getSize(hSpec);
        setMeasuredDimension(w, h);
        int cw = MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY);
        int ch = MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY);
        for (int i = 0; i < getChildCount(); i++) getChildAt(i).measure(cw, ch);
    }

    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int w = r - l, h = b - t;
        for (int i = 0; i < getChildCount(); i++) getChildAt(i).layout(i * w, 0, (i + 1) * w, h);
        if (changed) { scroller.forceFinished(true); scrollTo(page * w, 0); }
    }

    @Override public boolean onInterceptTouchEvent(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = lastX = e.getX(); downY = e.getY();
                dragging = false; swipedDown = false;
                if (!scroller.isFinished()) { scroller.abortAnimation(); dragging = true; }
                track(e);
                return dragging;
            case MotionEvent.ACTION_MOVE: {
                float dx = e.getX() - downX, dy = e.getY() - downY;
                if (Math.abs(dx) > touchSlop && Math.abs(dx) > Math.abs(dy)) {
                    dragging = true; lastX = e.getX(); track(e); return true;
                }
                if (dy > touchSlop * 3 && dy > Math.abs(dx) * 1.5f) { fireSwipeDown(); return true; }
                track(e);
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                dragging = false; recycle();
                break;
        }
        return false;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        gestures.onTouchEvent(e);
        track(e);
        int w = getWidth();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = lastX = e.getX(); downY = e.getY(); swipedDown = false;
                if (!scroller.isFinished()) scroller.abortAnimation();
                return true;
            case MotionEvent.ACTION_MOVE: {
                float dx = e.getX() - downX, dy = e.getY() - downY;
                if (!dragging) {
                    if (Math.abs(dx) > touchSlop && Math.abs(dx) > Math.abs(dy)) { dragging = true; lastX = e.getX(); }
                    else if (dy > touchSlop * 3 && dy > Math.abs(dx) * 1.5f) { fireSwipeDown(); return true; }
                }
                if (dragging) {
                    float delta = lastX - e.getX();
                    lastX = e.getX();
                    int max = Math.max(0, (getChildCount() - 1) * w);
                    int x = getScrollX();
                    if (x + delta < 0 || x + delta > max) delta *= 0.35f; // résistance élastique aux bords
                    scrollBy((int) delta, 0);
                }
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (dragging && w > 0) {
                    float v = 0;
                    if (vt != null) { vt.computeCurrentVelocity(1000); v = vt.getXVelocity(); }
                    int target = page;
                    float offset = getScrollX() - page * w;
                    if (v < -minFling || offset > w / 2f) target = page + 1;
                    else if (v > minFling || offset < -w / 2f) target = page - 1;
                    snapTo(target);
                }
                dragging = false; recycle();
                return true;
            }
        }
        return true;
    }

    private void fireSwipeDown() {
        if (swipedDown) return;
        swipedDown = true;
        if (listener != null) listener.onSwipeDown();
    }

    public void snapTo(int target) {
        int count = getChildCount();
        target = Math.max(0, Math.min(Math.max(0, count - 1), target));
        int dx = target * getWidth() - getScrollX();
        scroller.startScroll(getScrollX(), 0, dx, 0, 380);
        boolean changed = target != page;
        page = target;
        postInvalidateOnAnimation();
        if (listener != null && changed) listener.onPageChanged(page, count);
    }

    public void setPageImmediate(int p) {
        page = Math.max(0, Math.min(Math.max(0, getChildCount() - 1), p));
        scroller.forceFinished(true);
        scrollTo(page * getWidth(), 0);
        if (listener != null) listener.onPageChanged(page, getChildCount());
    }

    @Override public void computeScroll() {
        if (scroller.computeScrollOffset()) {
            scrollTo(scroller.getCurrX(), 0);
            postInvalidateOnAnimation();
        }
    }

    private void track(MotionEvent e) {
        if (vt == null) vt = VelocityTracker.obtain();
        vt.addMovement(e);
    }

    private void recycle() {
        if (vt != null) { vt.recycle(); vt = null; }
    }

    @Override public boolean shouldDelayChildPressedState() { return true; }

    /** Utilisé pour vider/reconstruire les pages. */
    public void setPages(View[] pages) {
        removeAllViews();
        for (View v : pages) addView(v);
        if (page >= pages.length) page = Math.max(0, pages.length - 1);
        requestLayout();
    }
}
