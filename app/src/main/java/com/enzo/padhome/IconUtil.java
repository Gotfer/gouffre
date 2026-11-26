package com.enzo.padhome;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.Drawable;

/** Transforme n'importe quelle icône Android en icône carrée arrondie façon iPadOS. */
public final class IconUtil {
    private IconUtil() {}

    /** Forme « squircle » proche des icônes Apple (courbes continues). */
    public static Path squircle(float size) {
        Path p = new Path();
        float r = size / 2f, n = 5f; // superellipse d'exposant 5
        int steps = 120;
        for (int i = 0; i <= steps; i++) {
            double t = 2 * Math.PI * i / steps;
            double c = Math.cos(t), s = Math.sin(t);
            float x = (float) (r + r * Math.signum(c) * Math.pow(Math.abs(c), 2 / n));
            float y = (float) (r + r * Math.signum(s) * Math.pow(Math.abs(s), 2 / n));
            if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
        }
        p.close();
        return p;
    }

    public static Bitmap make(Drawable d, int size) {
        Bitmap bmp = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas(bmp);
        Path shape = squircle(size);
        c.save();
        c.clipPath(shape);
        if (d instanceof AdaptiveIconDrawable) {
            AdaptiveIconDrawable a = (AdaptiveIconDrawable) d;
            // Les calques adaptatifs font 108dp pour une zone visible de 72dp : on agrandit de 1,5x.
            int extra = size / 4;
            Drawable bg = a.getBackground(), fg = a.getForeground();
            if (bg != null) { bg.setBounds(-extra, -extra, size + extra, size + extra); bg.draw(c); }
            else c.drawColor(Color.WHITE);
            if (fg != null) { fg.setBounds(-extra, -extra, size + extra, size + extra); fg.draw(c); }
        } else {
            // Ancienne icône : fond blanc et icône centrée, comme iPadOS le ferait.
            c.drawColor(Color.WHITE);
            int inset = Math.round(size * 0.14f);
            d.setBounds(inset, inset, size - inset, size - inset);
            d.draw(c);
        }
        c.restore();
        // Liseré très léger pour détacher l'icône du fond d'écran.
        Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(Math.max(1f, size / 96f));
        stroke.setColor(0x22000000);
        c.drawPath(shape, stroke);
        return bmp;
    }
}
