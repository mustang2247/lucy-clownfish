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

#include <stdlib.h>
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "CFCType.h"

struct CFCType {
    int   flags;
    char *specifier;
    int   indirection;
    void *parcel;
    char *c_string;
};

CFCType*
CFCType_new(void *parcel, const char *specifier, int indirection,
            const char *c_string)
{
    CFCType *self = (CFCType*)malloc(sizeof(CFCType));
    if (!self) { croak("malloc failed"); }
    return CFCType_init(self, parcel, specifier, indirection, c_string);
}

CFCType*
CFCType_init(CFCType *self, void *parcel, const char *specifier,
             int indirection, const char *c_string)
{
    self->parcel      = newSVsv((SV*)parcel);
    self->specifier   = savepv(specifier);
    self->indirection = indirection;
    self->c_string    = c_string ? savepv(c_string) : savepv("");
    return self;
}

void
CFCType_destroy(CFCType *self)
{
    Safefree(self->specifier);
    Safefree(self->c_string);
    SvREFCNT_dec(self->parcel);
    free(self);
}

void
CFCType_set_specifier(CFCType *self, const char *specifier)
{
    Safefree(self->specifier);
    self->specifier = savepv(specifier);
}

const char*
CFCType_get_specifier(CFCType *self)
{
    return self->specifier;
}

int
CFCType_get_indirection(CFCType *self)
{
    return self->indirection;
}

void*
CFCType_get_parcel(CFCType *self)
{
    return self->parcel;
}

void
CFCType_set_c_string(CFCType *self, const char *c_string)
{
    Safefree(self->c_string);
    self->c_string = savepv(c_string);
}

const char*
CFCType_to_c(CFCType *self)
{
    return self->c_string;
}
