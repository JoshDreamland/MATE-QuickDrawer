/*
	Copyright (C) 2010 The Free Software Foundation
	
	This file is part of the QuickDrawer Applet.
	
	QuickDrawer is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	QuickDrawer is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with QuickDrawer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtk/gtk.h>

static void do_colorshift (GdkPixbuf *dest, GdkPixbuf *src, int shift)
{
	int     i, j;
	int     width, height, has_alpha, srcrowstride, destrowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	int     val;
	guchar  r,g,b;

	has_alpha       = gdk_pixbuf_get_has_alpha (src);
	width           = gdk_pixbuf_get_width (src);
	height          = gdk_pixbuf_get_height (src);
	srcrowstride    = gdk_pixbuf_get_rowstride (src);
	destrowstride   = gdk_pixbuf_get_rowstride (dest);
	target_pixels   = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);
  
	for (i = 0; i < height; i++)
	{
		pixdest = target_pixels + i*destrowstride;
		pixsrc  = original_pixels + i*srcrowstride;
		for (j = 0; j < width; j++)
		{
			r            = *(pixsrc++);
			g            = *(pixsrc++);
			b            = *(pixsrc++);
			val          = r + shift;
			*(pixdest++) = CLAMP (val, 0, 255);
			val          = g + shift;
			*(pixdest++) = CLAMP (val, 0, 255);
			val          = b + shift;
			*(pixdest++) = CLAMP (val, 0, 255);
			
			if (has_alpha)
				*(pixdest++) = *(pixsrc++);
		}
	}
}


GdkPixbuf *make_bright_pixbuf (GdkPixbuf *pb)
{
	GdkPixbuf *bright_pixbuf;
	
	if (!pb)
		return pb;
	
	bright_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pb),
		gdk_pixbuf_get_has_alpha (pb),
		gdk_pixbuf_get_bits_per_sample (pb),
		gdk_pixbuf_get_width (pb),
		gdk_pixbuf_get_height (pb));
	do_colorshift (bright_pixbuf, pb, 30);
	
	return bright_pixbuf;
}

GtkWidget *gtk_image_brighten(GtkWidget* image) {
	return gtk_image_new_from_pixbuf(make_bright_pixbuf(gtk_image_get_pixbuf(GTK_IMAGE(image))));
}
