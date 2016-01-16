#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_DIALOG_SCAN_TYPE (ui_dialog_scan_get_type())
#define UI_DIALOG_SCAN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_DIALOG_SCAN_TYPE, UiDialogScan))
#define IS_UI_DIALOG_SCAN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_DIALOG_SCAN_TYPE))
#define UI_DIALOG_SCAN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_DIALOG_SCAN_TYPE, UiDialogScanClass))
#define IS_UI_DIALOG_SCAN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_DIALOG_SCAN_TYPE))
#define UI_DIALOG_SCAN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_DIALOG_SCAN_TYPE, UiDialogScanClass))

typedef struct _UiDialogScan UiDialogScan;
typedef struct _UiDialogScanPrivate UiDialogScanPrivate;
typedef struct _UiDialogScanClass UiDialogScanClass;

struct _UiDialogScan {
    GtkDialog parent_instance;

    /*< private >*/
    UiDialogScanPrivate *priv;
};

struct _UiDialogScanClass {
    GtkDialogClass parent_class;
};

GType ui_dialog_scan_get_type(void) G_GNUC_CONST;

GtkWidget *ui_dialog_scan_new(GtkWindow *parent);

GList *ui_dialog_scan_get_scanned_satellites(UiDialogScan *dialog); /*[element-type: gchar * */
GList *ui_dialog_scan_get_channel_results(UiDialogScan *dialog);    /*[element-type: ChannelData * */

GtkResponseType ui_dialog_scan_show(GtkWidget *parent);

G_END_DECLS
