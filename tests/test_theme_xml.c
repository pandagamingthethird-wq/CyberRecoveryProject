#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "theme_xml.h"

int main() {
    static cf_xml_node_t pool[64];
    const char *xml =
        "<page name=\"main\">"
        "  <object type=\"button\" x=\"10\" y=\"20\" text=\"Flash\" action=\"flash\"/>"
        "  <object type=\"button\" x=\"10\" y=\"80\" text=\"Backup &amp; Restore\" action=\"backup\"/>"
        "</page>";

    cf_xml_node_t *root = cf_xml_parse(xml, pool, 64);
    assert(root != NULL);
    assert(strcmp(root->tag, "page") == 0);
    assert(strcmp(cf_xml_get_attr(root, "name"), "main") == 0);
    assert(root->child_count == 2);

    cf_xml_node_t *btn1 = root->children[0];
    assert(strcmp(cf_xml_get_attr(btn1, "text"), "Flash") == 0);
    assert(strcmp(cf_xml_get_attr(btn1, "action"), "flash") == 0);
    assert(strcmp(cf_xml_get_attr(btn1, "x"), "10") == 0);

    cf_xml_node_t *btn2 = root->children[1];
    printf("btn2 text attr: '%s'\n", cf_xml_get_attr(btn2, "text"));
    assert(strcmp(cf_xml_get_attr(btn2, "text"), "Backup & Restore") == 0);

    assert(cf_xml_get_attr(root, "nonexistent") == NULL);

    /* malformed input shouldn't crash */
    assert(cf_xml_parse("not xml at all", pool, 64) == NULL);
    assert(cf_xml_parse("<unterminated", pool, 64) == NULL);
    assert(cf_xml_parse("", pool, 64) == NULL);

    printf("ALL THEME_XML TESTS PASSED\n");
    return 0;
}
