/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include <cmark.h>

#include "charmony.h"
#include "CFCCMan.h"
#include "CFCBase.h"
#include "CFCC.h"
#include "CFCClass.h"
#include "CFCDocuComment.h"
#include "CFCFunction.h"
#include "CFCMethod.h"
#include "CFCParamList.h"
#include "CFCSymbol.h"
#include "CFCType.h"
#include "CFCUri.h"
#include "CFCUtil.h"
#include "CFCVariable.h"

#ifndef true
    #define true 1
    #define false 0
#endif

static char*
S_man_create_name(CFCClass *klass);

static char*
S_man_create_synopsis(CFCClass *klass);

static char*
S_man_create_description(CFCClass *klass);

static char*
S_man_create_functions(CFCClass *klass);

static char*
S_man_create_methods(CFCClass *klass);

static char*
S_man_create_fresh_methods(CFCClass *klass, CFCClass *ancestor);

static char*
S_man_create_func(CFCClass *klass, CFCFunction *func, const char *full_sym);

static char*
S_man_create_param_list(CFCClass *klass, CFCFunction *func);

static char*
S_man_create_inheritance(CFCClass *klass);

static char*
S_md_to_man(CFCClass *klass, const char *md, int needs_indent);

static char*
S_nodes_to_man(CFCClass *klass, cmark_node *node, int needs_indent);

static char*
S_man_escape(const char *content);

char*
CFCCMan_create_man_page(CFCClass *klass) {
    if (!CFCSymbol_public((CFCSymbol*)klass)) { return NULL; }

    const char *class_name = CFCClass_get_class_name(klass);

    // Create NAME.
    char *name = S_man_create_name(klass);

    // Create SYNOPSIS.
    char *synopsis = S_man_create_synopsis(klass);

    // Create DESCRIPTION.
    char *description = S_man_create_description(klass);

    // Create CONSTRUCTORS.
    char *functions_man = S_man_create_functions(klass);

    // Create METHODS, possibly including an ABSTRACT METHODS section.
    char *methods_man = S_man_create_methods(klass);

    // Build an INHERITANCE section describing class ancestry.
    char *inheritance = S_man_create_inheritance(klass);

    // Put it all together.
    const char pattern[] =
        ".TH %s 3\n"
        "%s"
        "%s"
        "%s"
        "%s"
        "%s"
        "%s";
    char *man_page
        = CFCUtil_sprintf(pattern, class_name, name, synopsis, description,
                          functions_man, methods_man, inheritance);

    FREEMEM(name);
    FREEMEM(synopsis);
    FREEMEM(description);
    FREEMEM(functions_man);
    FREEMEM(methods_man);
    FREEMEM(inheritance);

    return man_page;
}

static char*
S_man_create_name(CFCClass *klass) {
    char *result = CFCUtil_strdup(".SH NAME\n");
    result = CFCUtil_cat(result, CFCClass_get_class_name(klass), NULL);

    const char *raw_brief = NULL;
    CFCDocuComment *docucom = CFCClass_get_docucomment(klass);
    if (docucom) {
        raw_brief = CFCDocuComment_get_brief(docucom);
    }
    if (raw_brief && raw_brief[0] != '\0') {
        char *brief = S_md_to_man(klass, raw_brief, false);
        result = CFCUtil_cat(result, " \\- ", brief, NULL);
        FREEMEM(brief);
    }
    else {
        result = CFCUtil_cat(result, "\n", NULL);
    }

    return result;
}

static char*
S_man_create_synopsis(CFCClass *klass) {
    CHY_UNUSED_VAR(klass);
    return CFCUtil_strdup("");
}

static char*
S_man_create_description(CFCClass *klass) {
    char *result  = CFCUtil_strdup("");

    CFCDocuComment *docucom = CFCClass_get_docucomment(klass);
    if (!docucom) { return result; }

    const char *raw_description = CFCDocuComment_get_long(docucom);
    if (!raw_description || raw_description[0] == '\0') { return result; }

    char *description = S_md_to_man(klass, raw_description, false);
    result = CFCUtil_cat(result, ".SH DESCRIPTION\n", description, NULL);
    FREEMEM(description);

    return result;
}

static char*
S_man_create_functions(CFCClass *klass) {
    CFCFunction **functions = CFCClass_functions(klass);
    char         *result    = CFCUtil_strdup("");

    for (int func_num = 0; functions[func_num] != NULL; func_num++) {
        CFCFunction *func = functions[func_num];
        if (!CFCFunction_public(func)) { continue; }

        if (result[0] == '\0') {
            result = CFCUtil_cat(result, ".SH FUNCTIONS\n", NULL);
        }

        const char *micro_sym = CFCFunction_micro_sym(func);
        result = CFCUtil_cat(result, ".TP\n.B ", micro_sym, "\n", NULL);

        const char *full_func_sym = CFCFunction_full_func_sym(func);
        char *function_man = S_man_create_func(klass, func, full_func_sym);
        result = CFCUtil_cat(result, function_man, NULL);
        FREEMEM(function_man);
    }

    return result;
}

static char*
S_man_create_methods(CFCClass *klass) {
    char *methods_man = CFCUtil_strdup("");
    char *result;

    for (CFCClass *ancestor = klass;
         ancestor;
         ancestor = CFCClass_get_parent(ancestor)
    ) {
        const char *class_name = CFCClass_get_class_name(ancestor);
        // Exclude methods inherited from Clownfish::Obj
        if (ancestor != klass && strcmp(class_name, "Clownfish::Obj") == 0) {
            break;
        }

        char *fresh_man = S_man_create_fresh_methods(klass, ancestor);
        if (fresh_man[0] != '\0') {
            if (ancestor == klass) {
                methods_man = CFCUtil_cat(methods_man, fresh_man, NULL);
            }
            else {
                methods_man
                    = CFCUtil_cat(methods_man, ".SS Methods inherited from ",
                                  class_name, "\n", fresh_man, NULL);
            }
        }
        FREEMEM(fresh_man);
    }

    if (methods_man[0] == '\0') {
        result = CFCUtil_strdup("");
    }
    else {
        result = CFCUtil_sprintf(".SH METHODS\n%s", methods_man);
    }

    FREEMEM(methods_man);
    return result;
}

static char*
S_man_create_fresh_methods(CFCClass *klass, CFCClass *ancestor) {
    CFCMethod  **fresh_methods = CFCClass_fresh_methods(klass);
    const char  *ancestor_name = CFCClass_get_class_name(ancestor);
    char        *result        = CFCUtil_strdup("");

    for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
        CFCMethod *method = fresh_methods[meth_num];
        if (!CFCMethod_public(method)) {
            continue;
        }

        const char *class_name = CFCMethod_get_class_name(method);
        if (strcmp(class_name, ancestor_name) != 0) {
            // The method is implementated in a subclass and already
            // documented.
            continue;
        }

        const char *macro_sym = CFCMethod_get_macro_sym(method);
        result = CFCUtil_cat(result, ".TP\n.BR ", macro_sym, NULL);
        if (CFCMethod_abstract(method)) {
            result = CFCUtil_cat(result, " \" (abstract)\"", NULL);
        }
        result = CFCUtil_cat(result, "\n", NULL);

        char *full_sym = CFCMethod_full_method_sym(method, klass);
        char *method_man = S_man_create_func(klass, (CFCFunction*)method,
                                             full_sym);
        result = CFCUtil_cat(result, method_man, NULL);
        FREEMEM(method_man);
        FREEMEM(full_sym);
    }

    return result;
}

static char*
S_man_create_func(CFCClass *klass, CFCFunction *func, const char *full_sym) {
    CFCType    *return_type   = CFCFunction_get_return_type(func);
    const char *return_type_c = CFCType_to_c(return_type);
    const char *incremented   = "";

    if (CFCType_incremented(return_type)) {
        incremented = " // incremented";
    }

    char *param_list = S_man_create_param_list(klass, func);

    const char *pattern =
        ".nf\n"
        ".fam C\n"
        "%s%s\n"
        ".BR %s %s\n"
        ".fam\n"
        ".fi\n";
    char *result = CFCUtil_sprintf(pattern, return_type_c, incremented,
                                   full_sym, param_list);

    FREEMEM(param_list);

    // Get documentation, which may be inherited.
    CFCDocuComment *docucomment = CFCFunction_get_docucomment(func);
    if (!docucomment) {
        const char *micro_sym = CFCFunction_micro_sym(func);
        CFCClass *parent = klass;
        while (NULL != (parent = CFCClass_get_parent(parent))) {
            CFCFunction *parent_func
                = (CFCFunction*)CFCClass_method(parent, micro_sym);
            if (!parent_func) { break; }
            docucomment = CFCFunction_get_docucomment(parent_func);
            if (docucomment) { break; }
        }
    }

    if (docucomment) {
        // Description
        const char *raw_desc = CFCDocuComment_get_description(docucomment);
        char *desc = S_md_to_man(klass, raw_desc, true);
        result = CFCUtil_cat(result, ".IP\n", desc, NULL);
        FREEMEM(desc);

        // Params
        const char **param_names
            = CFCDocuComment_get_param_names(docucomment);
        const char **param_docs
            = CFCDocuComment_get_param_docs(docucomment);
        if (param_names[0]) {
            result = CFCUtil_cat(result, ".RS\n", NULL);
            for (size_t i = 0; param_names[i] != NULL; i++) {
                char *doc = S_md_to_man(klass, param_docs[i], true);
                result = CFCUtil_cat(result, ".TP\n.I ", param_names[i],
                                     "\n", doc, NULL);
                FREEMEM(doc);
            }
            result = CFCUtil_cat(result, ".RE\n", NULL);
        }

        // Return value
        const char *retval_doc = CFCDocuComment_get_retval(docucomment);
        if (retval_doc && strlen(retval_doc)) {
            char *doc = S_md_to_man(klass, retval_doc, true);
            result = CFCUtil_cat(result, ".IP\n.B Returns:\n", doc, NULL);
            FREEMEM(doc);
        }
    }

    return result;
}

static char*
S_man_create_param_list(CFCClass *klass, CFCFunction *func) {
    CFCParamList  *param_list = CFCFunction_get_param_list(func);
    CFCVariable  **variables  = CFCParamList_get_variables(param_list);

    if (!variables[0]) {
        return CFCUtil_strdup("(void);");
    }

    const char *cfc_class = CFCBase_get_cfc_class((CFCBase*)func);
    int is_method = strcmp(cfc_class, "Clownfish::CFC::Model::Method") == 0;
    char *result = CFCUtil_strdup("(");

    for (int i = 0; variables[i]; ++i) {
        CFCVariable *variable = variables[i];
        CFCType     *type     = CFCVariable_get_type(variable);
        const char  *name     = CFCVariable_micro_sym(variable);
        char        *type_c;

        if (is_method && i == 0) {
            const char *struct_sym = CFCClass_full_struct_sym(klass);
            type_c = CFCUtil_sprintf("%s*", struct_sym);
        }
        else {
            type_c = CFCUtil_strdup(CFCType_to_c(type));
        }

        result = CFCUtil_cat(result, "\n.RB \"    ", type_c, " \" ", name,
                             NULL);

        if (variables[i+1] || CFCType_decremented(type)) {
            result = CFCUtil_cat(result, " \"", NULL);
            if (variables[i+1]) {
                result = CFCUtil_cat(result, ",", NULL);
            }
            if (CFCType_decremented(type)) {
                result = CFCUtil_cat(result, " // decremented", NULL);
            }
            result = CFCUtil_cat(result, "\"", NULL);
        }

        FREEMEM(type_c);
    }

    result = CFCUtil_cat(result, "\n);", NULL);

    return result;
}

static char*
S_man_create_inheritance(CFCClass *klass) {
    CFCClass *ancestor = CFCClass_get_parent(klass);
    char     *result   = CFCUtil_strdup("");

    if (!ancestor) { return result; }

    const char *class_name = CFCClass_get_class_name(klass);
    result = CFCUtil_cat(result, ".SH INHERITANCE\n", class_name, NULL);
    while (ancestor) {
        const char *ancestor_name = CFCClass_get_class_name(ancestor);
        result = CFCUtil_cat(result, " is a ", ancestor_name, NULL);
        ancestor = CFCClass_get_parent(ancestor);
    }
    result = CFCUtil_cat(result, ".\n", NULL);

    return result;
}

static char*
S_md_to_man(CFCClass *klass, const char *md, int needs_indent) {
    cmark_node *doc = cmark_parse_document(md, strlen(md));
    char *result = S_nodes_to_man(klass, doc, needs_indent);
    cmark_node_free(doc);

    return result;
}

static char*
S_nodes_to_man(CFCClass *klass, cmark_node *node, int needs_indent) {
    char *result = CFCUtil_strdup("");
    int has_indent = needs_indent;
    int has_vspace = true;

    while (node) {
        cmark_node_type type = cmark_node_get_type(node);

        switch (type) {
            case CMARK_NODE_DOCUMENT: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, children_man, NULL);
                FREEMEM(children_man);
                break;
            }

            case CMARK_NODE_PARAGRAPH: {
                if (needs_indent && !has_indent) {
                    result = CFCUtil_cat(result, ".IP\n", NULL);
                    has_indent = true;
                }
                else if (!needs_indent && has_indent) {
                    result = CFCUtil_cat(result, ".P\n", NULL);
                    has_indent = false;
                }
                else if (!has_vspace) {
                    result = CFCUtil_cat(result, "\n", NULL);
                }

                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, children_man, "\n", NULL);
                FREEMEM(children_man);

                has_vspace = false;

                break;
            }

            case CMARK_NODE_BLOCK_QUOTE: {
                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RS\n", NULL);
                }

                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child, true);
                result = CFCUtil_cat(result, ".IP\n", children_man, NULL);
                FREEMEM(children_man);

                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RE\n", NULL);
                    has_indent = false;
                }
                else {
                    has_indent = true;
                }

                break;
            }

            case CMARK_NODE_ITEM: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child, true);
                result = CFCUtil_cat(result, ".IP \\(bu\n", children_man,
                                     NULL);
                FREEMEM(children_man);
                break;
            }

            case CMARK_NODE_LIST: {
                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RS\n", NULL);
                }

                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, children_man, NULL);
                FREEMEM(children_man);

                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RE\n", NULL);
                    has_indent = false;
                }
                else {
                    has_indent = true;
                }

                break;
            }

            case CMARK_NODE_HEADER: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, ".SS\n", children_man, "\n", NULL);
                FREEMEM(children_man);
                has_indent = false;
                has_vspace = true;
                break;
            }

            case CMARK_NODE_CODE_BLOCK: {
                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RS\n", NULL);
                }

                const char *content = cmark_node_get_literal(node);
                char *escaped = S_man_escape(content);
                result = CFCUtil_cat(result, ".IP\n.nf\n.fam C\n", escaped,
                                     ".fam\n.fi\n", NULL);
                FREEMEM(escaped);

                if (needs_indent) {
                    result = CFCUtil_cat(result, ".RE\n", NULL);
                    has_indent = false;
                }
                else {
                    has_indent = true;
                }

                break;
            }

            case CMARK_NODE_HTML:
                CFCUtil_warn("HTML not supported in man pages");
                break;

            case CMARK_NODE_HRULE:
                break;

            case CMARK_NODE_TEXT: {
                const char *content = cmark_node_get_literal(node);
                char *escaped = S_man_escape(content);
                result = CFCUtil_cat(result, escaped, NULL);
                FREEMEM(escaped);
                break;
            }

            case CMARK_NODE_LINEBREAK:
                result = CFCUtil_cat(result, "\n.br\n", NULL);
                break;

            case CMARK_NODE_SOFTBREAK:
                result = CFCUtil_cat(result, "\n", NULL);
                break;

            case CMARK_NODE_CODE: {
                const char *content = cmark_node_get_literal(node);
                char *escaped = S_man_escape(content);
                result = CFCUtil_cat(result, "\\FC", escaped, "\\F[]", NULL);
                FREEMEM(escaped);
                break;
            }

            case CMARK_NODE_INLINE_HTML: {
                const char *html = cmark_node_get_literal(node);
                CFCUtil_warn("HTML not supported in man pages: %s", html);
                break;
            }

            case CMARK_NODE_LINK: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                const char *url = cmark_node_get_url(node);
                if (CFCUri_is_clownfish_uri(url)) {
                    if (children_man[0] != '\0') {
                        result = CFCUtil_cat(result, children_man, NULL);
                    }
                    else {
                        CFCUri *uri_obj = CFCUri_new(url, klass);
                        char *link_text = CFCC_link_text(uri_obj, klass);
                        if (link_text) {
                            result = CFCUtil_cat(result, link_text, NULL);
                            FREEMEM(link_text);
                        }
                        CFCBase_decref((CFCBase*)uri_obj);
                    }
                }
                else {
                    result = CFCUtil_cat(result, "\n.UR ", url, "\n",
                                         children_man, "\n.UE\n",
                                         NULL);
                }
                FREEMEM(children_man);
                break;
            }

            case CMARK_NODE_IMAGE:
                CFCUtil_warn("Images not supported in man pages");
                break;

            case CMARK_NODE_STRONG: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, "\\fB", children_man, "\\f[]",
                                     NULL);
                FREEMEM(children_man);
                break;
            }

            case CMARK_NODE_EMPH: {
                cmark_node *child = cmark_node_first_child(node);
                char *children_man = S_nodes_to_man(klass, child,
                                                    needs_indent);
                result = CFCUtil_cat(result, "\\fI", children_man, "\\f[]",
                                     NULL);
                FREEMEM(children_man);
                break;
            }

            default:
                CFCUtil_die("Invalid cmark node type: %d", type);
                break;
        }

        node = cmark_node_next(node);
    }

    return result;
}

static char*
S_man_escape(const char *content) {
    size_t  len        = strlen(content);
    size_t  result_len = 0;
    size_t  result_cap = len + 256;
    char   *result     = (char*)MALLOCATE(result_cap + 1);

    for (size_t i = 0; i < len; i++) {
        const char *subst      = content + i;
        size_t      subst_size = 1;

        switch (content[i]) {
            case '\\':
                // Escape backslash.
                subst      = "\\e";
                subst_size = 2;
                break;
            case '-':
                // Escape hyphen.
                subst      = "\\-";
                subst_size = 2;
                break;
            case '.':
                // Escape dot at start of line.
                if (i == 0 || content[i-1] == '\n') {
                    subst      = "\\&.";
                    subst_size = 3;
                }
                break;
            case '\'':
                // Escape single quote at start of line.
                if (i == 0 || content[i-1] == '\n') {
                    subst      = "\\&'";
                    subst_size = 3;
                }
                break;
            default:
                break;
        }

        if (result_len + subst_size > result_cap) {
            result_cap += 256;
            result = (char*)REALLOCATE(result, result_cap + 1);
        }

        memcpy(result + result_len, subst, subst_size);
        result_len += subst_size;
    }

    result[result_len] = '\0';

    return result;
}

