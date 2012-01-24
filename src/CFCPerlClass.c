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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCPerlClass.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCMethod.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCFunction.h"
#include "CFCDocuComment.h"
#include "CFCSymbol.h"
#include "CFCPerlPod.h"
#include "CFCPerlMethod.h"
#include "CFCPerlConstructor.h"

struct CFCPerlClass {
    CFCBase base;
    CFCParcel *parcel;
    char *class_name;
    CFCClass *client;
    char *xs_code;
    CFCPerlPod *pod_spec;
    char **meth_aliases;
    char **meth_names;
    size_t num_methods;
    char **cons_aliases;
    char **cons_inits;
    size_t num_cons;
};

static CFCPerlClass **registry = NULL;
static size_t registry_size = 0;
static size_t registry_cap  = 0;

const static CFCMeta CFCPERLCLASS_META = {
    "Clownfish::CFC::Binding::Perl::Class",
    sizeof(CFCPerlClass),
    (CFCBase_destroy_t)CFCPerlClass_destroy
};

CFCPerlClass*
CFCPerlClass_new(CFCParcel *parcel, const char *class_name, CFCClass *client,
                 const char *xs_code, CFCPerlPod *pod_spec) {
    CFCPerlClass *self = (CFCPerlClass*)CFCBase_allocate(&CFCPERLCLASS_META);
    return CFCPerlClass_init(self, parcel, class_name, client, xs_code,
                             pod_spec);
}

CFCPerlClass*
CFCPerlClass_init(CFCPerlClass *self, CFCParcel *parcel,
                  const char *class_name, CFCClass *client,
                  const char *xs_code, CFCPerlPod *pod_spec) {
    CFCUTIL_NULL_CHECK(parcel);
    CFCUTIL_NULL_CHECK(class_name);
    self->parcel = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->client = (CFCClass*)CFCBase_incref((CFCBase*)client);
    self->class_name = CFCUtil_strdup(class_name);
    self->pod_spec = (CFCPerlPod*)CFCBase_incref((CFCBase*)pod_spec);
    self->xs_code = xs_code ? CFCUtil_strdup(xs_code) : NULL;
    self->meth_aliases = NULL;
    self->meth_names   = NULL;
    self->num_methods  = 0;
    self->cons_aliases = NULL;
    self->cons_inits   = NULL;
    self->num_cons     = 0;
    return self;
}

void
CFCPerlClass_destroy(CFCPerlClass *self) {
    CFCBase_decref((CFCBase*)self->parcel);
    CFCBase_decref((CFCBase*)self->client);
    CFCBase_decref((CFCBase*)self->pod_spec);
    FREEMEM(self->class_name);
    FREEMEM(self->xs_code);
    for (size_t i = 0; i < self->num_methods; i++) {
        FREEMEM(self->meth_aliases);
        FREEMEM(self->meth_names);
    }
    FREEMEM(self->meth_aliases);
    FREEMEM(self->meth_names);
    for (size_t i = 0; i < self->num_cons; i++) {
        FREEMEM(self->cons_aliases);
        FREEMEM(self->cons_inits);
    }
    FREEMEM(self->cons_aliases);
    FREEMEM(self->cons_inits);
    CFCBase_destroy((CFCBase*)self);
}

static int
S_compare_cfcperlclass(const void *va, const void *vb) {
    CFCPerlClass *a = *(CFCPerlClass**)va;
    CFCPerlClass *b = *(CFCPerlClass**)vb;
    return strcmp(a->class_name, b->class_name);
}

void
CFCPerlClass_add_to_registry(CFCPerlClass *self) {
    if (registry_size == registry_cap) {
        size_t new_cap = registry_cap + 10;
        registry = (CFCPerlClass**)REALLOCATE(registry,
                                              (new_cap + 1) * sizeof(CFCPerlClass*));
        for (size_t i = registry_cap; i <= new_cap; i++) {
            registry[i] = NULL;
        }
        registry_cap = new_cap;
    }
    CFCPerlClass *existing = CFCPerlClass_singleton(self->class_name);
    if (existing) {
        CFCUtil_die("Class '%s' already registered", self->class_name);
    }
    registry[registry_size] = (CFCPerlClass*)CFCBase_incref((CFCBase*)self);
    registry_size++;
    qsort(registry, registry_size, sizeof(CFCPerlClass*),
          S_compare_cfcperlclass);
}

CFCPerlClass*
CFCPerlClass_singleton(const char *class_name) {
    CFCUTIL_NULL_CHECK(class_name);
    for (size_t i = 0; i < registry_size; i++) {
        CFCPerlClass *existing = registry[i];
        if (strcmp(class_name, existing->class_name) == 0) {
            return existing;
        }
    }
    return NULL;
}

CFCPerlClass**
CFCPerlClass_registry() {
    return registry;
}

void
CFCPerlClass_clear_registry(void) {
    for (size_t i = 0; i < registry_size; i++) {
        CFCBase_decref((CFCBase*)registry[i]);
    }
    FREEMEM(registry);
    registry_size = 0;
    registry_cap  = 0;
    registry      = NULL;
}

void
CFCPerlClass_bind_method(CFCPerlClass *self, const char *alias,
                         const char *method) {
    size_t size = (self->num_methods + 1) * sizeof(char*);
    self->meth_aliases = (char**)REALLOCATE(self->meth_aliases, size);
    self->meth_names   = (char**)REALLOCATE(self->meth_names,   size);
    self->meth_aliases[self->num_methods] = (char*)CFCUtil_strdup(alias);
    self->meth_names[self->num_methods]   = (char*)CFCUtil_strdup(method);
    self->num_methods++;
    if (!self->client) {
        CFCUtil_die("Can't bind_method %s -- can't find client for %s",
                    alias, self->class_name);
    }
}

void
CFCPerlClass_bind_constructor(CFCPerlClass *self, const char *alias,
                              const char *initializer) {
    alias       = alias       ? alias       : "new";
    initializer = initializer ? initializer : "init";
    size_t size = (self->num_cons + 1) * sizeof(char*);
    self->cons_aliases = (char**)REALLOCATE(self->cons_aliases, size);
    self->cons_inits   = (char**)REALLOCATE(self->cons_inits,   size);
    self->cons_aliases[self->num_cons] = (char*)CFCUtil_strdup(alias);
    self->cons_inits[self->num_cons]   = (char*)CFCUtil_strdup(initializer);
    self->num_cons++;
    if (!self->client) {
        CFCUtil_die("Can't bind_constructor %s -- can't find client for %s",
                    alias, self->class_name);
    }
}

CFCPerlMethod**
CFCPerlClass_method_bindings(CFCPerlClass *self) {
    CFCClass       *client     = self->client;
    const char     *class_name = self->class_name;
    size_t          num_bound  = 0;
    CFCPerlMethod **bound 
        = (CFCPerlMethod**)CALLOCATE(1, sizeof(CFCPerlMethod*));

    // Iterate over the list of methods to be bound.
    for (size_t i = 0; i < self->num_methods; i++) {
        const char *meth_name = self->meth_names[i];
        CFCMethod *method = CFCClass_method(client, meth_name);

        // Safety checks against bad specs, excess binding code or private
        // methods.
        if (!method) {
            CFCUtil_die("Can't find method '%s' for class '%s'", meth_name,
                        class_name);
        }
        else if (!CFCMethod_novel(method)) {
            CFCUtil_die("Binding spec'd for method '%s' in class '%s', "
                        "but it's overridden and should be bound via the "
                        "parent class", meth_name, class_name);
        }
        else if (CFCSymbol_private((CFCSymbol*)method)) {
            CFCUtil_die("Can't bind private method '%s' in class '%s', ",
                        meth_name, class_name);
        }

        /* Create an XSub binding for each override.  Each of these directly
         * calls the implementing function, rather than invokes the method on
         * the object using VTable method dispatch.  Doing things this way
         * allows SUPER:: invocations from Perl-space to work properly.
         */
        CFCClass **descendants = CFCClass_tree_to_ladder(client);
        for (size_t j = 0; descendants[j] != NULL; j++) {
            CFCClass *descendant = descendants[j];
            CFCMethod *real_method
                = CFCClass_fresh_method(descendant, meth_name);
            if (!real_method) { continue; }

            // Create the binding, add it to the array.
            CFCPerlMethod *meth_binding
                = CFCPerlMethod_new(real_method, self->meth_aliases[i]);
            size_t size = (num_bound + 2) * sizeof(CFCPerlMethod*);
            bound = (CFCPerlMethod**)REALLOCATE(bound, size);
            bound[num_bound] = meth_binding;
            num_bound++;
            bound[num_bound] = NULL;
        }
    }

    return bound;
}

CFCPerlConstructor**
CFCPerlClass_constructor_bindings(CFCPerlClass *self) {
    CFCClass   *client     = self->client;
    size_t      num_bound  = 0;
    CFCPerlConstructor **bound 
        = (CFCPerlConstructor**)CALLOCATE(1, sizeof(CFCPerlConstructor*));

    // Iterate over the list of constructors to be bound.
    for (size_t i = 0; i < self->num_cons; i++) {
        // Create the binding, add it to the array.
        CFCPerlConstructor *cons_binding
            = CFCPerlConstructor_new(client, self->cons_aliases[i],
                                     self->cons_inits[i]);
        size_t size = (num_bound + 2) * sizeof(CFCPerlConstructor*);
        bound = (CFCPerlConstructor**)REALLOCATE(bound, size);
        bound[num_bound] = cons_binding;
        num_bound++;
        bound[num_bound] = NULL;
    }

    return bound;
}

char*
CFCPerlClass_create_pod(CFCPerlClass *self) {
    CFCPerlPod *pod_spec   = self->pod_spec;
    const char *class_name = self->class_name;
    CFCClass   *client     = self->client;
    if (!pod_spec) {
        return NULL;
    }
    if (!client) {
        CFCUtil_die("No client for %s", class_name);
    }
    CFCDocuComment *docucom = CFCClass_get_docucomment(client);
    if (!docucom) {
        CFCUtil_die("No DocuComment for %s", class_name);
    }

    // Get the class's brief description.
    const char *raw_brief = CFCDocuComment_get_brief(docucom);
    char *brief = CFCPerlPod_perlify_doc_text(pod_spec, raw_brief);

    // Get the class's long description.
    const char *raw_description = CFCPerlPod_get_description(pod_spec);
    if (!raw_description || !strlen(raw_description)) {
        raw_description = CFCDocuComment_get_long(docucom);
    }
    char *description = CFCPerlPod_perlify_doc_text(pod_spec, raw_description);

    // Create SYNOPSIS.
    const char *raw_synopsis = CFCPerlPod_get_synopsis(pod_spec);
    char *synopsis = CFCUtil_strdup("");
    if (raw_synopsis && strlen(raw_synopsis)) {
        synopsis = CFCUtil_cat(synopsis, "=head1 SYNOPSIS\n\n", raw_synopsis,
                               "\n", NULL);
    }

    // Create CONSTRUCTORS.
    char *constructor_pod = CFCPerlPod_constructors_pod(pod_spec, client);

    // Create METHODS, possibly including an ABSTRACT METHODS section.
    char *methods_pod = CFCPerlPod_methods_pod(pod_spec, client);

    // Build an INHERITANCE section describing class ancestry.
    char *inheritance = CFCUtil_strdup("");
    if (CFCClass_get_parent(client)) {
        inheritance = CFCUtil_cat(inheritance, "=head1 INHERITANCE\n\n",
                                  class_name, NULL);
        CFCClass *ancestor = client;
        while (NULL != (ancestor = CFCClass_get_parent(ancestor))) {
            const char *ancestor_klass = CFCClass_get_class_name(ancestor);
            if (CFCPerlClass_singleton(ancestor_klass)) {
                inheritance = CFCUtil_cat(inheritance, " isa L<",
                                          ancestor_klass, ">", NULL);
            }
            else {
                inheritance = CFCUtil_cat(inheritance, " isa ",
                                          ancestor_klass, NULL);
            }
        }
        inheritance = CFCUtil_cat(inheritance, ".\n", NULL);
    }

    // Put it all together.
    const char pattern[] = 
    "# Auto-generated file -- DO NOT EDIT!!!!!\n"
    "\n"
    "# Licensed to the Apache Software Foundation (ASF) under one or more\n"
    "# contributor license agreements.  See the NOTICE file distributed with\n"
    "# this work for additional information regarding copyright ownership.\n"
    "# The ASF licenses this file to You under the Apache License, Version 2.0\n"
    "# (the \"License\"); you may not use this file except in compliance with\n"
    "# the License.  You may obtain a copy of the License at\n"
    "#\n"
    "#     http://www.apache.org/licenses/LICENSE-2.0\n"
    "#\n"
    "# Unless required by applicable law or agreed to in writing, software\n"
    "# distributed under the License is distributed on an \"AS IS\" BASIS,\n"
    "# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
    "# See the License for the specific language governing permissions and\n"
    "# limitations under the License.\n"
    "\n"
    "=head1 NAME\n"
    "\n"
    "%s - %s\n"
    "\n"
    "%s\n"
    "\n"
    "=head1 DESCRIPTION\n"
    "\n"
    "%s\n"
    "\n"
    "%s\n"
    "\n"
    "%s\n"
    "\n"
    "%s\n"
    "\n"
    "=cut\n"
    "\n";

    size_t size = sizeof(pattern)
                  + strlen(class_name)
                  + strlen(brief)
                  + strlen(synopsis)
                  + strlen(description)
                  + strlen(constructor_pod)
                  + strlen(methods_pod)
                  + strlen(inheritance)
                  + 20;

    char *pod = (char*)MALLOCATE(size);
    sprintf(pod, pattern, class_name, brief, synopsis, description,
            constructor_pod, methods_pod, inheritance);

    FREEMEM(brief);
    FREEMEM(synopsis);
    FREEMEM(description);
    FREEMEM(constructor_pod);
    FREEMEM(methods_pod);
    FREEMEM(inheritance);

    return pod;
}

CFCClass*
CFCPerlClass_get_client(CFCPerlClass *self) {
    return self->client;
}

const char*
CFCPerlClass_get_class_name(CFCPerlClass *self) {
    return self->class_name;
}

const char*
CFCPerlClass_get_xs_code(CFCPerlClass *self) {
    return self->xs_code;
}

CFCPerlPod*
CFCPerlClass_get_pod_spec(CFCPerlClass *self) {
    return self->pod_spec;
}
