#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CHANNEL_FAVOURITES_DIALOG_TYPE (channel_favourites_dialog_get_type())
#define CHANNEL_FAVOURITES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), CHANNEL_FAVOURITES_DIALOG_TYPE, ChannelFavouritesDialog))
#define IS_CHANNEL_FAVOURITES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), CHANNEL_FAVOURITES_DIALOG_TYPE))
#define CHANNEL_FAVOURITES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CHANNEL_FAVOURITES_DIALOG_TYPE))
#define CHANNEL_FAVOURITES_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), CHANNEL_FAVOURITES_DIALOG_TYPE, ChannelFavouritesDialogClass))

typedef struct _ChannelFavouritesDialog ChannelFavouritesDialog;
typedef struct _ChannelFavouritesDialogClass ChannelFavouritesDialogClass;

typedef enum {
    FAV_ROW_ID,
    FAV_ROW_TITLE,
    FAV_N_ROWS
} FavouritesListEntry;

struct _ChannelFavouritesDialog {
    GtkDialog parent_instance;
};

struct _ChannelFavouritesDialogClass {
    GtkDialogClass parent_class;
};

GType channel_favourites_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *channel_favourites_dialog_new(GtkWindow *parent);

void channel_favourites_dialog_set_parent(ChannelFavouritesDialog *dialog, GtkWindow *parent);

typedef void (*CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY)(gpointer userdata);
void channel_favourites_dialog_set_update_notify(ChannelFavouritesDialog *dialog,
        CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY cb, gpointer userdata);
gboolean channel_favourites_dialog_write_favourite_lists(ChannelFavouritesDialog *dialog);
void channel_favourites_dialog_set_current_list(ChannelFavouritesDialog *dialog, guint32 list_id);

G_END_DECLS
