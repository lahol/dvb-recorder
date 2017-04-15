#include "favourites-dialog.h"
#include <dvbrecorder/channel-db.h>
#include <dvbrecorder/dvbrecorder.h>
#include "channel-list.h"
#include "ui-dialog-scan.h"
#include "ui-dialogs-run.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdio.h>

typedef struct _ChannelFavList ChannelFavList;
typedef struct _ChannelFavListEntry ChannelFavListEntry;

enum ChannelFavListFlag {
    CFL_FLAG_NEW = 1<<0,
    CFL_FLAG_CHANGED = 1 << 1,
    CFL_FLAG_REMOVED = 1 << 2
};

struct _ChannelFavList {
    ChannelDBList data;
    guint32 flags;

    GList *entries; /* [element-type: ChannelFavListEntry] */
};

struct _ChannelFavListEntry {
    ChannelData data;
    guint32 flags;
    ChannelFavList *parent;
};

struct _ChannelFavouritesDialogPrivate {
    GtkListStore *channels_all;
    GtkListStore *channels_selected;
    GtkListStore *favourites;

    GtkWidget *channel_fav_list;
    GtkWidget *channel_all_list;
    GtkWidget *favourites_combo;

    GtkWindow *parent;
    DVBRecorder *recorder;

    GList *fav_lists; /* [element-type: ChannelFavLists] */
    ChannelFavList *current_fav_list;

    CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY update_notify_cb;
    gpointer update_notify_data;
};

void channel_favourites_dialog_add_favourite_to_store(ChannelFavouritesDialog *self, ChannelDBList *list);
void channel_favourites_dialog_set_active_list(ChannelFavouritesDialog *self, guint32 list_id);

gint _fav_list_find_entry_by_id(ChannelFavListEntry *entry, guint32 id)
{
    if (entry->data.id == id)
        return 0;
    return 1;
}

void channel_favourites_dialog_add_favourites_list_cb(ChannelDBList *entry, GList **list)
{
    if (!entry || entry->id == 0)
        return;
    ChannelFavList *fl = g_malloc0(sizeof(ChannelFavList));
    channel_db_list_copy(&fl->data, entry);

    *list = g_list_prepend(*list, fl);
}

void channel_favourites_add_favourites_cb(ChannelData *channel, GList **list)
{
    ChannelFavListEntry *fle = g_malloc0(sizeof(ChannelFavListEntry));
    channel_data_copy(&fle->data, channel);

    *list = g_list_prepend(*list, fle);
}

void channel_favourites_dialog_read_favourites(ChannelFavouritesDialog *dialog)
{
    channel_db_list_foreach((CHANNEL_DB_LIST_FOREACH_CALLBACK)channel_favourites_dialog_add_favourites_list_cb,
            &dialog->priv->fav_lists);
    dialog->priv->fav_lists = g_list_reverse(dialog->priv->fav_lists);

    GList *tmp;
    for (tmp = dialog->priv->fav_lists; tmp != NULL; tmp = g_list_next(tmp)) {
        channel_db_foreach(((ChannelFavList *)tmp->data)->data.id,
                (CHANNEL_DB_FOREACH_CALLBACK)channel_favourites_add_favourites_cb,
                &((ChannelFavList *)tmp->data)->entries);
        ((ChannelFavList *)tmp->data)->entries = g_list_reverse(((ChannelFavList *)tmp->data)->entries);
    }
}

void channel_favourites_dialog_write_favourites(ChannelFavList *list)
{
    fprintf(stderr, "write favourites\n");
    g_return_if_fail(list != NULL);
    GList *tmp;
    ChannelFavListEntry *entry;

    gint pos = 0;

    channel_db_start_transaction();

    for (tmp = list->entries; tmp != NULL; tmp = g_list_next(tmp)) {
        entry = (ChannelFavListEntry *)tmp->data;
        if (entry->flags & CFL_FLAG_REMOVED) {
            channel_db_list_remove_entry(&list->data, &entry->data);
        }
        else if (entry->flags & CFL_FLAG_NEW) {
            channel_db_list_add_entry(&list->data, &entry->data, ++pos);
        }
        else {
            channel_db_list_update_entry(&list->data, &entry->data, ++pos);
        }
    }

    channel_db_commit_transaction();
}

gboolean channel_favourites_dialog_write_favourite_lists(ChannelFavouritesDialog *dialog)
{
    fprintf(stderr, "write lists\n");
    g_return_val_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(dialog), FALSE);

    GList *tmp;
    ChannelFavList *list;
    gboolean changes = FALSE;
    
    channel_db_start_transaction();

    for (tmp = dialog->priv->fav_lists; tmp != NULL; tmp = g_list_next(tmp)) {
        list = (ChannelFavList *)tmp->data;
        if (list->flags & (CFL_FLAG_CHANGED | CFL_FLAG_NEW)) {
            changes = TRUE;
            channel_favourites_dialog_write_favourites(list);
        }
    }

    channel_db_commit_transaction();

    return changes;
}

G_DEFINE_TYPE(ChannelFavouritesDialog, channel_favourites_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_RECORDER,
    N_PROPERTIES
};

enum {
    TARGET_ID,
    TARGET_STRING,
    TARGET_ROW
};

void _channel_favourites_dialog_free_channel_fav_list_entry(ChannelFavListEntry *entry)
{
    channel_data_clear(&entry->data);
    g_free(entry);
}

void _channel_favourites_dialog_free_channel_fav_list(ChannelFavList *list)
{
    channel_db_list_clear(&list->data);
    g_list_free_full(list->entries,
            (GDestroyNotify)_channel_favourites_dialog_free_channel_fav_list_entry);

    g_free(list);
}

static void channel_favourites_dialog_dispose(GObject *gobject)
{
    ChannelFavouritesDialog *self = CHANNEL_FAVOURITES_DIALOG(gobject);

    g_list_free_full(self->priv->fav_lists,
            (GDestroyNotify)_channel_favourites_dialog_free_channel_fav_list);
    self->priv->fav_lists = NULL;
    self->priv->current_fav_list = NULL;

    G_OBJECT_CLASS(channel_favourites_dialog_parent_class)->dispose(gobject);
}

static void channel_favourites_dialog_finalize(GObject *gobject)
{
    G_OBJECT_CLASS(channel_favourites_dialog_parent_class)->finalize(gobject);
}

static void channel_favourites_dialog_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *spec)
{
    ChannelFavouritesDialog *dialog = CHANNEL_FAVOURITES_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            channel_favourites_dialog_set_parent(dialog, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_RECORDER:
            dialog->priv->recorder = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void channel_favourites_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    ChannelFavouritesDialog *dialog = CHANNEL_FAVOURITES_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            g_value_set_object(value, dialog->priv->parent);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void channel_favourites_dialog_class_init(ChannelFavouritesDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = channel_favourites_dialog_dispose;
    gobject_class->finalize = channel_favourites_dialog_finalize;
    gobject_class->set_property = channel_favourites_dialog_set_property;
    gobject_class->get_property = channel_favourites_dialog_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
            PROP_RECORDER,
            g_param_spec_pointer("recorder",
                "DVBRecorder",
                "The recorder handle",
                G_PARAM_WRITABLE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(ChannelFavouritesDialogPrivate));
}

void favourites_dialog_response_cb(GtkDialog *dialog, gint response_id, gpointer data)
{
    if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT) {
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
}

void _channel_favourites_drag_data_get(GtkWidget *widget, GdkDragContext *context,
        GtkSelectionData *sel_data, guint info, guint time, gpointer userdata)
{
    fprintf(stderr, "drag data get\n");
    GtkTreeIter iter;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    gboolean rv;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    rv = gtk_tree_selection_get_selected(selection, &store, &iter);

    if (rv == FALSE) {
        fprintf(stderr, "nothing selected\n");
        return;
    }

    guint32 id;
    gtk_tree_model_get(store, &iter, CHNL_ROW_ID, &id, -1);
    
    gtk_selection_data_set(sel_data,
            gdk_atom_intern("channel id pointer", FALSE),
            8,
            (void *)&id,
            sizeof(id));
}

void _channel_favourites_favourites_drag_data_get(GtkWidget *widget, GdkDragContext *context,
        GtkSelectionData *sel_data, guint info, guint time, gpointer userdata)
{
    fprintf(stderr, "favourites drag data get\n");

    GtkTreeIter iter;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    gboolean rv;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    rv = gtk_tree_selection_get_selected(selection, &store, &iter);

    if (rv == FALSE) {
        fprintf(stderr, "nothing selected\n");
        return;
    }

    gtk_selection_data_set(sel_data,
            gdk_atom_intern("channel fav row", FALSE),
            8,
            (void *)&iter,
            sizeof(iter));
    fprintf(stderr, "get iter: %d %p %p %p\n", iter.stamp, iter.user_data, iter.user_data2, iter.user_data3);
}

void _channel_favourites_drag_data_received(GtkWidget *widget, GdkDragContext *context,
        gint x, gint y, GtkSelectionData *sel_data, guint info, guint time, ChannelFavouritesDialog *dialog)
{
/*    g_signal_stop_emission_by_name(G_OBJECT(widget), "drag-data-received");*/
    fprintf(stderr, "drag data received for %p\n", widget);
    GtkTreeModel *store;
    GtkTreeIter iter, sibling;
    GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition pos;
    GdkDragAction suggested_action;

    store = gtk_tree_view_get_model(tree_view);
    if (GTK_IS_TREE_MODEL_FILTER(store)) {
        store = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(store));
    }

    /*suggested_action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(context),
                "gtk-tree-view-status-pending"));*/
    suggested_action = gdk_drag_context_get_suggested_action(context);
    fprintf(stderr, "suggested action: %d (def: %d, copy: %d, move: %d, link: %d\n", suggested_action,
            GDK_ACTION_DEFAULT, GDK_ACTION_COPY, GDK_ACTION_MOVE, GDK_ACTION_LINK);
    if (suggested_action) {
        /* get data due to request in drag_motion not drag_drop so just call drag_status, do not paste data
         * see: gtktreeview.c:gtk_tree_view_drag_data_received()*/
        gtk_tree_view_get_dest_row_at_pos(tree_view, x, y, &path, &pos);
        fprintf(stderr, "path: %p, pos: %d\n", path, pos);
        if (path == NULL)
            suggested_action = 0;
        else {
            switch (pos) {
                case GTK_TREE_VIEW_DROP_BEFORE:
                case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
                    break;
                case GTK_TREE_VIEW_DROP_AFTER:
                case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
                    if (!gtk_tree_model_get_iter(store, &iter, path) ||
                            !gtk_tree_model_iter_next(store, &iter))
                    {
                    }
                    else {
                        gtk_tree_path_next(path);
                    }
                    break;
            }
        }
        if (suggested_action) {
            if (!gtk_tree_drag_dest_row_drop_possible(GTK_TREE_DRAG_DEST(store), path, sel_data)) {
                fprintf(stderr, "drop not possible\n");
                suggested_action = 0;
            }
        }

        gdk_drag_status(context, suggested_action, time);

        if (path)
            gtk_tree_path_free(path);

        if (suggested_action == 0)
            gtk_tree_view_set_drag_dest_row(tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
    }

    const guchar *data = gtk_selection_data_get_data(sel_data);
    gint insert_position = -1;
    gint *indices = NULL;
    GtkTreeIter source_iter;

    fprintf(stderr, "selection data: %p\n", data);
    ChannelData *chnl_data = NULL;
    guint chnl_id = 0;

    if (data) {
        if (info != TARGET_ROW )
            chnl_data = channel_db_get_channel(*((guint32 *)data));
        else {
            memcpy(&source_iter, data, sizeof(GtkTreeIter));
            gtk_tree_model_get(store, &source_iter, CHNL_ROW_ID, &chnl_id, -1);
        fprintf(stderr, "recv iter: %d %p %p %p\n", source_iter.stamp, source_iter.user_data, source_iter.user_data2, source_iter.user_data3);
        }
    }

    if (chnl_data != NULL) {
        /* FIXME: fails when moving inside treeview */
        if (dialog->priv->current_fav_list && 
                g_list_find_custom(dialog->priv->current_fav_list->entries,
                GUINT_TO_POINTER(chnl_data->id),
                (GCompareFunc)_fav_list_find_entry_by_id)) {
            fprintf(stderr, "already in list\n");
            gtk_drag_finish(context, FALSE, FALSE, time);
            channel_data_free(chnl_data);
            return;
        }

        if (!gtk_tree_view_get_dest_row_at_pos(tree_view, x, y, &path, &pos)) {
            gtk_list_store_append(GTK_LIST_STORE(store), &iter);
        }
        else {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &sibling, path);
            indices = gtk_tree_path_get_indices(path);
            if (indices) {
                insert_position = indices[0];
            }
            switch (pos) {
                case GTK_TREE_VIEW_DROP_BEFORE:
                case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
                    gtk_list_store_insert_before(GTK_LIST_STORE(store), &iter, &sibling);
                    break;
                case GTK_TREE_VIEW_DROP_AFTER:
                case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
                    gtk_list_store_insert_after(GTK_LIST_STORE(store), &iter, &sibling);
                    if (insert_position >= 0)
                        ++insert_position;
                    break;
            }
            gtk_tree_path_free(path);
        }
        gtk_drag_finish(context, TRUE, FALSE, time);
    }
    else if (info == TARGET_ROW) {
        if (!gtk_tree_view_get_dest_row_at_pos(tree_view, x, y, &path, &pos)) {
            fprintf(stderr, "dest not found\n");
            gtk_list_store_move_before(GTK_LIST_STORE(store), &source_iter, NULL);
        }
        else {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &sibling, path);
            indices = gtk_tree_path_get_indices(path);
            if (indices) {
                insert_position = indices[0];
            }
            switch (pos) {
                case GTK_TREE_VIEW_DROP_BEFORE:
                case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
                    gtk_list_store_move_before(GTK_LIST_STORE(store), &source_iter, &sibling);
                    break;
                case GTK_TREE_VIEW_DROP_AFTER:
                case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
                    gtk_list_store_move_after(GTK_LIST_STORE(store), &source_iter, &sibling);
                    if (insert_position >= 0)
                        ++insert_position;
                    break;
            }
            gtk_tree_path_free(path);
        }
        gtk_drag_finish(context, TRUE, FALSE, time);
    }
    else {
        gtk_drag_finish(context, FALSE, FALSE, time);
    }


    if (chnl_data != NULL) {
        fprintf(stderr, "insert at position: %d\n", insert_position);
        gtk_list_store_set(GTK_LIST_STORE(store), &iter,
                CHNL_ROW_ID, chnl_data->id,
                CHNL_ROW_TITLE, chnl_data->name,
                CHNL_ROW_SOURCE, chnl_data->signalsource,
                CHNL_ROW_FOREGROUND, chnl_data->flags & CHNL_FLAG_DIRTY ? "gray" : NULL,
                -1);
        if (dialog->priv->current_fav_list) {
            ChannelFavListEntry *entry = g_malloc0(sizeof(ChannelFavListEntry));
            channel_data_copy(&entry->data, chnl_data);
            entry->flags = CFL_FLAG_NEW;
            dialog->priv->current_fav_list->entries =
                g_list_insert(dialog->priv->current_fav_list->entries, entry, insert_position);
            dialog->priv->current_fav_list->flags |= CFL_FLAG_CHANGED;
        }

        channel_data_free(chnl_data);
    }
    else if (info == TARGET_ROW) {
        GList *link = NULL;
        gpointer link_data;
        if (dialog->priv->current_fav_list &&
                (link = g_list_find_custom(dialog->priv->current_fav_list->entries,
                        GUINT_TO_POINTER(chnl_id),
                        (GCompareFunc)_fav_list_find_entry_by_id))) {
            link_data = link->data;
            fprintf(stderr, "move from %d to %d\n", g_list_position(dialog->priv->current_fav_list->entries, link), insert_position);
            dialog->priv->current_fav_list->entries =
                g_list_delete_link(dialog->priv->current_fav_list->entries, link);
            if (insert_position >= 0)
                dialog->priv->current_fav_list->entries =
                    g_list_insert(dialog->priv->current_fav_list->entries, link_data, insert_position);
            else {
                GList *tmp, *first_removed = NULL;
                for (tmp = dialog->priv->current_fav_list->entries; tmp; tmp = g_list_next(tmp)) {
                    if (((ChannelFavListEntry *)tmp->data)->flags & CFL_FLAG_REMOVED) {
                        first_removed = tmp;
                        break;
                    }
                }
                dialog->priv->current_fav_list->entries =
                    g_list_insert_before(dialog->priv->current_fav_list->entries, first_removed, link_data);
            }
            dialog->priv->current_fav_list->flags |= CFL_FLAG_CHANGED;
        }
    }
}

static void favourites_dialog_add_list_dialog_run(ChannelFavouritesDialog *parent)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Add List"),
            GTK_WINDOW(parent),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Add"), GTK_RESPONSE_OK,
            _("Cancel"), GTK_RESPONSE_CANCEL,
            NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 3);
    gtk_widget_show(entry);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_OK) {
        const gchar *title = gtk_entry_get_text(GTK_ENTRY(entry));
        if (title == NULL || title[0] == '\0')
            goto done;
        ChannelFavList *list = g_malloc0(sizeof(ChannelFavList));
        list->data.title = g_strdup(title);
        list->data.id = channel_db_list_add(title);
        list->flags |= CFL_FLAG_NEW;

        parent->priv->fav_lists = g_list_append(parent->priv->fav_lists, list);
        channel_favourites_dialog_add_favourite_to_store(parent, &list->data);
    }

done:
    gtk_widget_destroy(dialog);
}

void channel_favourites_dialog_add_favourite_to_store(ChannelFavouritesDialog *self, ChannelDBList *list)
{
    g_return_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(self));
    g_return_if_fail(list != NULL);

    /* ignore all channels */
    if (list->id == 0)
        return;

    GtkTreeIter iter;
    gtk_list_store_append(GTK_LIST_STORE(self->priv->favourites), &iter);

    gtk_list_store_set(GTK_LIST_STORE(self->priv->favourites), &iter,
            FAV_ROW_ID, list->id,
            FAV_ROW_TITLE, list->title,
            -1);
}

void channel_favourites_dialog_fill_favourites_store(ChannelFavouritesDialog *self)
{
    GList *tmp;
    for (tmp = self->priv->fav_lists; tmp != NULL; tmp = g_list_next(tmp)) {
        channel_favourites_dialog_add_favourite_to_store(self,
                &((ChannelFavList *)tmp->data)->data);
    }
}

void channel_favourites_dialog_add_channel_to_favourites_store(ChannelFavouritesDialog *self, ChannelData *data)
{
    g_return_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(self));
    g_return_if_fail(data != NULL);

    GtkTreeIter iter;
    gtk_list_store_append(GTK_LIST_STORE(self->priv->channels_selected), &iter);

    gtk_list_store_set(GTK_LIST_STORE(self->priv->channels_selected), &iter,
            CHNL_ROW_ID, data->id,
            CHNL_ROW_TITLE, data->name,
            CHNL_ROW_SOURCE, data->signalsource,
            CHNL_ROW_FOREGROUND, data->flags & CHNL_FLAG_DIRTY ? "gray" : NULL,
            -1);
}

void channel_favourites_dialog_reset_favourites_channel_store(ChannelFavouritesDialog *self)
{
    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(self->priv->channel_fav_list));

    gtk_widget_set_sensitive(GTK_WIDGET(tv), self->priv->current_fav_list ? TRUE : FALSE);
    
    gtk_list_store_clear(self->priv->channels_selected);

    if (self->priv->current_fav_list == NULL)
        return;

    GList *tmp;

    for (tmp = self->priv->current_fav_list->entries; tmp != NULL; tmp = g_list_next(tmp)) {
        channel_favourites_dialog_add_channel_to_favourites_store(self,
                &((ChannelFavListEntry *)tmp->data)->data);
    }
}

void channel_favourites_dialog_update_channel_list(ChannelFavouritesDialog *self)
{
    channel_list_clear(CHANNEL_LIST(self->priv->channel_all_list));
    channel_db_foreach(0, (CHANNEL_DB_FOREACH_CALLBACK)channel_list_fill_cb, self->priv->channel_all_list);
}

void channel_favourites_dialog_set_active_list(ChannelFavouritesDialog *self, guint32 list_id)
{
    if (self->priv->current_fav_list && self->priv->current_fav_list->data.id == list_id)
        return;

    GList *tmp;
    for (tmp = self->priv->fav_lists; tmp != NULL; tmp = g_list_next(tmp)) {
        if (((ChannelFavList *)tmp->data)->data.id == list_id) {
            self->priv->current_fav_list = (ChannelFavList *)tmp->data;
            channel_favourites_dialog_reset_favourites_channel_store(self);
        }
    }
}

static void channel_favourites_dialog_list_changed(GtkComboBox *box, ChannelFavouritesDialog *dialog)
{
    GtkTreeIter iter;
    GValue value = G_VALUE_INIT;
    gtk_combo_box_get_active_iter(box, &iter);
    gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->priv->favourites),
            &iter, FAV_ROW_ID, &value);
    guint32 id = g_value_get_uint(&value);

    channel_favourites_dialog_set_active_list(dialog, id);
    g_value_unset(&value);
}

static void channel_favourites_dialog_scan_button_clicked(ChannelFavouritesDialog *dialog)
{
    GtkResponseType resp = ui_dialog_scan_show(GTK_WIDGET(dialog), dialog->priv->recorder);
    if (resp == GTK_RESPONSE_OK) {
        channel_favourites_dialog_update_channel_list(dialog);
        if (dialog->priv->update_notify_cb)
            dialog->priv->update_notify_cb(dialog->priv->update_notify_data);
    }
}

struct FavListRemoveData {
    ChannelFavouritesDialog *self;
    GtkTreePath *path;
};

static void channel_favourites_dialog_favourites_list_remove_activate(struct FavListRemoveData *data)
{
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(data->self->priv->channels_selected), &iter, data->path))
        goto done;

    guint chnl_id = 0;
    gtk_tree_model_get(GTK_TREE_MODEL(data->self->priv->channels_selected), &iter, CHNL_ROW_ID, &chnl_id, -1);

    gtk_list_store_remove(data->self->priv->channels_selected, &iter);

    GList *link = NULL;
    if (data->self->priv->current_fav_list &&
            (link = g_list_find_custom(data->self->priv->current_fav_list->entries,
                GUINT_TO_POINTER(chnl_id),
                (GCompareFunc)_fav_list_find_entry_by_id))) {
        ((ChannelFavListEntry *)link->data)->flags |= CFL_FLAG_REMOVED;
        gpointer link_data = link->data;
        data->self->priv->current_fav_list->entries =
            g_list_delete_link(data->self->priv->current_fav_list->entries, link);
        data->self->priv->current_fav_list->entries =
            g_list_append(data->self->priv->current_fav_list->entries, link_data);

        data->self->priv->current_fav_list->flags |= CFL_FLAG_CHANGED;
    }

done:
    if (data) {
        if (data->path)
            gtk_tree_path_free(data->path);
        g_free(data);
    }
}

static gboolean channel_favourites_dialog_favourites_list_button_press_event(
        ChannelFavouritesDialog *self, GdkEventButton *event, GtkWidget *widget)
{
    /* call default handler first (selection change) */
    GtkWidgetClass *cls = GTK_WIDGET_GET_CLASS(widget);
    gboolean result = FALSE;
    if (cls && cls->button_press_event)
        result = cls->button_press_event(widget, event);

    if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
        GtkTreePath *path = NULL;
/*        gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget),
                event->x, event->y, &bx, &by);*/
        if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL))
            return result;

        GtkWidget *popup = gtk_menu_new();
        GtkWidget *item = gtk_menu_item_new_with_label(_("Remove"));
        struct FavListRemoveData *data = g_malloc0(sizeof(struct FavListRemoveData));
        data->self = self;
        data->path = path;
        g_signal_connect_swapped(G_OBJECT(item), "activate",
                G_CALLBACK(channel_favourites_dialog_favourites_list_remove_activate), data);
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

        gtk_widget_show_all(popup);
#if GTK_CHECK_VERSION(3,22,0)
        gtk_menu_popup_at_pointer(GTK_MENU(popup), (GdkEvent *)event);
#else
        gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL, event->button, event->time);
#endif

        return TRUE;
    }
    return result;
}

static void channel_favourites_dialog_init(ChannelFavouritesDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            CHANNEL_FAVOURITES_DIALOG_TYPE, ChannelFavouritesDialogPrivate);

    gtk_window_set_title(GTK_WINDOW(self), _("Channel lists"));
    gtk_window_set_default_size(GTK_WINDOW(self), 600, 400);

    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Apply"), GTK_RESPONSE_APPLY,
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);

    /* build models */
    self->priv->favourites = gtk_list_store_new(FAV_N_ROWS,
            G_TYPE_UINT, G_TYPE_STRING);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    GtkWidget *hbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);

    self->priv->favourites_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(self->priv->favourites));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(self->priv->favourites_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(self->priv->favourites_combo), renderer, "text", FAV_ROW_TITLE, NULL);

    g_signal_connect(G_OBJECT(self->priv->favourites_combo), "changed",
            G_CALLBACK(channel_favourites_dialog_list_changed), self);

    gtk_box_pack_start(GTK_BOX(hbox), self->priv->favourites_combo, TRUE, TRUE, 3);

    GtkWidget *button = gtk_button_new_with_label("Add list");
    g_signal_connect_swapped(G_OBJECT(button), "clicked",
            G_CALLBACK(favourites_dialog_add_list_dialog_run), self);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);

    button = gtk_button_new_with_label("Scan Channels");
    g_signal_connect_swapped(G_OBJECT(button), "clicked",
            G_CALLBACK(channel_favourites_dialog_scan_button_clicked), self);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
    
    gtk_widget_show_all(hbox);

    gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 3);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    GtkTreeView *tv;

    static GtkTargetEntry targetentries[] = {
        { "FAV_ENTRIES_ROW", GTK_TARGET_SAME_WIDGET, TARGET_ROW },
        { "STRING", GTK_TARGET_SAME_APP, TARGET_STRING },
        { "INTEGER", GTK_TARGET_SAME_APP, TARGET_ID }
    };
 
    self->priv->channel_fav_list = channel_list_new(FALSE);
    self->priv->channels_selected = channel_list_get_list_store(CHANNEL_LIST(self->priv->channel_fav_list));
    /* drag and drop */
    tv = channel_list_get_tree_view(CHANNEL_LIST(self->priv->channel_fav_list));
    fprintf(stderr, "Favlist: %p\n", tv);
    gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tv), targetentries, 3,
            GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tv), GDK_BUTTON1_MASK, targetentries, 3,
            GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(tv), "drag-data-received",
            G_CALLBACK(_channel_favourites_drag_data_received), self);
    g_signal_connect(G_OBJECT(tv), "drag-data-get",
            G_CALLBACK(_channel_favourites_favourites_drag_data_get), NULL);
    g_signal_connect_swapped(G_OBJECT(tv), "button-press-event",
            G_CALLBACK(channel_favourites_dialog_favourites_list_button_press_event), self);
    gtk_box_pack_start(GTK_BOX(hbox), self->priv->channel_fav_list, TRUE, TRUE, 3);
    gtk_widget_set_sensitive(GTK_WIDGET(tv), FALSE);

    self->priv->channel_all_list = channel_list_new(TRUE);
    self->priv->channels_all = channel_list_get_list_store(CHANNEL_LIST(self->priv->channel_all_list));
    /* drag and drop */
    tv = channel_list_get_tree_view(CHANNEL_LIST(self->priv->channel_all_list));
    fprintf(stderr, "Channellist: %p\n", tv);
    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tv), GDK_BUTTON1_MASK, targetentries, 3,
            GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(tv), "drag-data-get",
            G_CALLBACK(_channel_favourites_drag_data_get), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), self->priv->channel_all_list, TRUE, TRUE, 3);

    gtk_widget_show_all(hbox);

    gtk_box_pack_start(GTK_BOX(content_area), hbox, TRUE, TRUE, 3);

    /* fill store */
    channel_favourites_dialog_update_channel_list(self);

    channel_favourites_dialog_read_favourites(self);
    channel_favourites_dialog_fill_favourites_store(self);
}

GtkWidget *channel_favourites_dialog_new(GtkWindow *parent)
{
    return g_object_new(CHANNEL_FAVOURITES_DIALOG_TYPE, "parent", parent, NULL);
}

void channel_favourites_dialog_set_parent(ChannelFavouritesDialog *dialog, GtkWindow *parent)
{
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
}

void channel_favourites_dialog_set_update_notify(ChannelFavouritesDialog *dialog,
        CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY cb, gpointer userdata)
{   
    g_return_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(dialog));

    dialog->priv->update_notify_cb = cb;
    dialog->priv->update_notify_data = userdata;
}

gboolean channel_favourites_dialog_find_list_by_id(ChannelFavouritesDialog *dialog, guint32 list_id, GtkTreeIter *match)
{
    g_return_val_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(dialog), FALSE);

    GtkTreeIter iter;
    gboolean valid;
    guint32 row_id;

    for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->priv->favourites), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->priv->favourites), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(dialog->priv->favourites), &iter, FAV_ROW_ID, &row_id, -1);
        if (row_id == list_id) {
            if (match)
                *match = iter;
            return TRUE;
        }
    }

    return FALSE;
}

void channel_favourites_dialog_set_current_list(ChannelFavouritesDialog *dialog, guint32 list_id)
{
    g_return_if_fail(IS_CHANNEL_FAVOURITES_DIALOG(dialog));

    GtkTreeIter iter;

    if (!channel_favourites_dialog_find_list_by_id(dialog, list_id, &iter))
        return;

    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->priv->favourites_combo), &iter);
}

