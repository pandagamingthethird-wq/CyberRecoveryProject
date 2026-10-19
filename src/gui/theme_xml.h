#ifndef CF_THEME_XML_H
#define CF_THEME_XML_H

#include <stdbool.h>

/*
 * theme_xml — a minimal, hand-rolled parser for a deliberately small XML
 * subset, so a theme package (a "ui.zip" equivalent) can define pages and
 * buttons declaratively instead of them being hardcoded in ui_theme.c.
 *
 * THIS IS NOT a general XML parser. It does not handle: namespaces, CDATA,
 * entity references beyond &amp;/&lt;/&gt;/&quot;/&apos;, processing
 * instructions, DOCTYPE, or nested nesting beyond what cf_xml_node_t's
 * fixed child array allows. It's sized for TWRP-style theme files, which
 * use a flat, predictable structure — not for parsing arbitrary XML from
 * untrusted sources. If you need more, link an actual XML library
 * instead of extending this.
 *
 * Example input this parses:
 *   <page name="main">
 *     <object type="button" x="10" y="20" text="Flash" action="flash"/>
 *     <object type="button" x="10" y="80" text="Backup" action="backup"/>
 *   </page>
 */

#define CF_XML_MAX_ATTRS 8
#define CF_XML_MAX_CHILDREN 32
#define CF_XML_MAX_NAME 32
#define CF_XML_MAX_VALUE 128

typedef struct {
    char name[CF_XML_MAX_NAME];
    char value[CF_XML_MAX_VALUE];
} cf_xml_attr_t;

typedef struct cf_xml_node {
    char tag[CF_XML_MAX_NAME];
    cf_xml_attr_t attrs[CF_XML_MAX_ATTRS];
    int attr_count;
    struct cf_xml_node *children[CF_XML_MAX_CHILDREN];
    int child_count;
} cf_xml_node_t;

/* Parses `xml_text` into a tree of cf_xml_node_t, allocated from a fixed
 * pool (no malloc — recovery environments avoid heap fragmentation in
 * long-running processes, same reasoning TWRP's own GUI code follows).
 * Returns the root node, or NULL on a malformed/oversized document.
 * `pool` must have at least pool_capacity nodes of storage; the parser
 * never allocates more nodes than fit in it. */
cf_xml_node_t *cf_xml_parse(const char *xml_text, cf_xml_node_t *pool, int pool_capacity);

/* Returns the attribute value for `attr_name`, or NULL if absent. */
const char *cf_xml_get_attr(const cf_xml_node_t *node, const char *attr_name);

#endif /* CF_THEME_XML_H */
