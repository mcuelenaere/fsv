#include <gtk/gtk.h>


void
on_file_change_root_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_file_save_settings_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_file_save_settings_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_file_exit_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_vis_mapv_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_vis_treev_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_color_by_nodetype_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_color_by_timestamp_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_color_by_wildcards_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_color_setup_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_help_contents_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_help_about_fsv_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_back_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_cd_root_button_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_cd_up_button_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_birdseye_view_togglebutton_toggled  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
