// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef MOVIAN_HAVE_GUMBO
#include "duktape.h"
#include "gumbo/gumbo.h"
#include <cstring>

namespace movian::plugins
{
namespace
{
void write_node(duk_context *ctx, const GumboNode &node, unsigned depth, unsigned &remaining)
{
    if (depth > 128 || remaining == 0)
        duk_error(ctx, DUK_ERR_RANGE_ERROR, "HTML tree exceeds plugin limits");
    --remaining;
    duk_require_stack(ctx, 12);
    duk_push_object(ctx);
    const GumboVector *children = nullptr;
    const char *name;
    int type;
    switch (node.type)
    {
    case GUMBO_NODE_DOCUMENT:
        name = "#document";
        type = 9;
        children = &node.v.document.children;
        break;
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE:
    {
        const auto &element = node.v.element;
        name = gumbo_normalized_tagname(element.tag);
        type = 1;
        children = &element.children;
        duk_push_array(ctx);
        for (unsigned i = 0; i < element.attributes.length; ++i)
        {
            const auto &attribute = *static_cast<const GumboAttribute *>(element.attributes.data[i]);
            duk_push_object(ctx);
            duk_push_string(ctx, attribute.name);
            duk_put_prop_string(ctx, -2, "name");
            duk_push_string(ctx, attribute.value);
            duk_put_prop_string(ctx, -2, "value");
            duk_put_prop_index(ctx, -2, i);
        }
        duk_put_prop_string(ctx, -2, "attributes");
        break;
    }
    default:
        type = node.type == GUMBO_NODE_COMMENT ? 8 :
               node.type == GUMBO_NODE_PROCESSING_INSTRUCTION ? 7 : 3;
        name = type == 8 ? "#comment" : type == 7 ? "#processing-instruction" : "#text";
        duk_push_string(ctx, node.v.text.text);
        duk_put_prop_string(ctx, -2, "text");
        break;
    }
    duk_push_int(ctx, type);
    duk_put_prop_string(ctx, -2, "type");
    if (type == 1 && !*name)
    {
        auto tag = node.v.element.original_tag;
        gumbo_tag_from_original_text(&tag);
        duk_push_lstring(ctx, tag.data, tag.length);
    }
    else
        duk_push_string(ctx, name);
    duk_put_prop_string(ctx, -2, "name");
    if (!children)
        return;
    duk_push_array(ctx);
    for (unsigned i = 0; i < children->length; ++i)
    {
        write_node(ctx, *static_cast<const GumboNode *>(children->data[i]), depth + 1, remaining);
        duk_put_prop_index(ctx, -2, i);
    }
    duk_put_prop_string(ctx, -2, "children");
}

duk_ret_t write_document(duk_context *ctx, void *data)
{
    unsigned remaining = 12000;
    write_node(ctx, *static_cast<const GumboNode *>(data), 0, remaining);
    return 1;
}

duk_ret_t parse_html(duk_context *ctx)
{
    duk_size_t size = 0;
    const char *text = duk_require_lstring(ctx, 0, &size);
    if (size > 256 * 1024)
        return duk_error(ctx, DUK_ERR_RANGE_ERROR, "HTML exceeds plugin input limit");
    GumboOptions options = kGumboDefaultOptions;
    options.max_errors = 0;
    auto *output = gumbo_parse_with_options(&options, text, size);
    if (!output->root)
    {
        gumbo_destroy_output(&options, output);
        return duk_error(ctx, DUK_ERR_RANGE_ERROR, "HTML exceeds parser limits");
    }
    const auto result = duk_safe_call(ctx, write_document, output->document, 0, 1);
    gumbo_destroy_output(&options, output);
    if (result != DUK_EXEC_SUCCESS)
        return duk_throw(ctx);
    return 1;
}
}

void install_html(duk_context *ctx)
{
    duk_push_c_function(ctx, parse_html, 1);
    duk_put_global_string(ctx, "__movianParseHtml");
}
}
#endif
