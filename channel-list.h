#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/channels.h>

G_BEGIN_DECLS

#define CHANNEL_LIST_TYPE (channel_list_get_type())
#define CHANNEL_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), CHANNEL_LIST_TYPE, ChannelList))
#define IS_CHANNEL_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), CHANNEL_LIST_TYPE))
#define CHANNEL_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), CHANNEL_LIST_TYPE, ChannelListClass))
#define IS_CHANNEL_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CHANNEL_LIST_TYPE))
#define CHANNEL_LIST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), CHANNEL_LIST_TYPE, ChannelListClass))

typedef struct _ChannelList ChannelList;
typedef struct _ChannelListClass ChannelListClass;

typedef enum {
    CHNL_ROW_ID,
    CHNL_ROW_TITLE,
    CHNL_ROW_SOURCE,
    CHNL_ROW_FOREGROUND,
    CHNL_N_ROWS
} ChannelListStoreEntry;

struct _ChannelList {
    GtkBin parent_instance;
};

struct _ChannelListClass {
    GtkBinClass parent_class;
};

GType channel_list_get_type(void) G_GNUC_CONST;

GtkWidget *channel_list_new(gboolean filterable);

void channel_list_set_change_on_select(ChannelList *channel_list, gboolean change_on_select);

void channel_list_set_model(ChannelList *channel_list, GtkTreeModel *model);
GtkTreeSelection *channel_list_get_selection(ChannelList *channel_list);
GtkListStore *channel_list_get_list_store(ChannelList *channel_list);
GtkTreeView *channel_list_get_tree_view(ChannelList *channel_list);

void channel_list_fill_cb(ChannelData *data, ChannelList *channel_list);
void channel_list_clear(ChannelList *channel_list);
void channel_list_reset(ChannelList *channel_list, guint32 list_id);
void channel_list_set_active_signal_source(ChannelList *channel_list, const gchar *signal_source);
const gchar *channel_list_get_active_signal_source(ChannelList *channel_list);

#define CHANNEL_LIST_ID_INVALID ((guint32)0)

typedef enum {
    ChannelListSelectionNone = (0),
    ChannelListSelectionActivate = (1 << 0),
    ChannelListSelectionScrollToCell = (1 << 1)
} ChannelListSetSelectionFlags;
void channel_list_set_channel_selection(ChannelList *channel_list, guint32 id, ChannelListSetSelectionFlags flags);
guint32 channel_list_get_channel_from_path(ChannelList *channel_list, GtkTreePath *path);
guint32 channel_list_get_channel_selection(ChannelList *channel_list);
GtkTreePath *channel_list_get_path_at_pos(ChannelList *channel_list, gint x, gint y);

G_END_DECLS
