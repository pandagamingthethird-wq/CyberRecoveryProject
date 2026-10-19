#include "ui_loop.h"
#include <stdlib.h>
#include <string.h>

static int attr_int(const cf_xml_node_t *node, const char *name, int default_value) {
    const char *v = cf_xml_get_attr(node, name);
    if (!v) return default_value;
    return atoi(v);
}

static void render_page(const cf_ui_loop_config_t *config, const cf_xml_node_t *page) {
    cf_fb_clear(config->fb, config->bg_r, config->bg_g, config->bg_b);

    for (int i = 0; i < page->child_count; i++) {
        const cf_xml_node_t *obj = page->children[i];
        const char *type = cf_xml_get_attr(obj, "type");
        if (!type || strcmp(type, "button") != 0) continue;

        int x = attr_int(obj, "x", 0);
        int y = attr_int(obj, "y", 0);
        int w = attr_int(obj, "w", 60);
        int h = attr_int(obj, "h", 20);

        cf_fb_fill_rect(config->fb, x, y, w, h,
                         config->button_r, config->button_g, config->button_b);

        const char *text = cf_xml_get_attr(obj, "text");
        if (text) {
            /* simple centering within the button, text at scale 1 */
            int text_px_width = (int)strlen(text) * 6; /* matches fb_backend's glyph advance */
            int tx = x + (w - text_px_width) / 2;
            if (tx < x) tx = x;
            int ty = y + (h - 7) / 2;
            if (ty < y) ty = y;
            cf_fb_draw_text(config->fb, tx, ty, text, 1,
                             config->text_r, config->text_g, config->text_b);
        }
    }
}

/* Finds the button object whose (x,y,w,h) rect contains (px,py), or
 * NULL if none does. Later-drawn (later in child order) buttons win on
 * overlap, matching normal top-to-bottom hit-testing expectations. */
static const cf_xml_node_t *find_button_at(const cf_xml_node_t *page, int px, int py) {
    const cf_xml_node_t *hit = NULL;
    for (int i = 0; i < page->child_count; i++) {
        const cf_xml_node_t *obj = page->children[i];
        const char *type = cf_xml_get_attr(obj, "type");
        if (!type || strcmp(type, "button") != 0) continue;

        int x = attr_int(obj, "x", 0);
        int y = attr_int(obj, "y", 0);
        int w = attr_int(obj, "w", 60);
        int h = attr_int(obj, "h", 20);

        if (px >= x && px < x + w && py >= y && py < y + h) {
            hit = obj;
        }
    }
    return hit;
}

static void dispatch_action(const cf_ui_loop_config_t *config, const char *action_name) {
    if (!action_name) return;
    for (int i = 0; i < config->binding_count; i++) {
        if (strcmp(config->bindings[i].action_name, action_name) == 0) {
            config->bindings[i].callback(config->user_data);
            return;
        }
    }
    /* No matching binding — not fatal (a theme can reference an action
     * this build doesn't implement yet), silently ignored by design. */
}

int cf_ui_run_page(const cf_ui_loop_config_t *config, const cf_xml_node_t *page, int max_events) {
    render_page(config, page);

    int processed = 0;
    while (processed < max_events) {
        cf_input_event_t ev;
        if (!config->read_event(config->event_source_ctx, &ev)) break;
        processed++;

        if (ev.type == CF_INPUT_TOUCH_UP) {
            const cf_xml_node_t *btn = find_button_at(page, ev.x, ev.y);
            if (btn) {
                const char *action = cf_xml_get_attr(btn, "action");
                dispatch_action(config, action);
            }
        }
    }
    return processed;
}
