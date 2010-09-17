/*
 * Copyright 2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Daniel Kopecek" <dkopecek@redhat.com>
 */
#include <errno.h>
#include <string.h>
#include "common/assume.h"
#include "oval_sexp.h"
#include "oval_probe_impl.h"
#include "oval_definitions_impl.h"
#include "public/oval_system_characteristics.h"
#include "probes/public/probe-api.h"
#include "oval_probe_int.h"

static struct oval_value *oval_object_getentval(struct oval_object *obj, const char *name)
{
        struct oval_value   *val;
        struct oval_entity  *ent;
        struct oval_object_content *con;
        struct oval_object_content_iterator *cit;
        char *ent_name;

        cit = oval_object_get_object_contents(obj);
        val = NULL;

	while (oval_object_content_iterator_has_more(cit)) {
                con = oval_object_content_iterator_next(cit);

                if (oval_object_content_get_type(con) != OVAL_OBJECTCONTENT_ENTITY)
                        continue;

                ent = oval_object_content_get_entity(con);
                ent_name = oval_entity_get_name(ent);

                if (strcmp(ent_name, name) != 0)
                        continue;

                val = oval_entity_get_value(ent);
                break;
        }

        oval_object_content_iterator_free(cit);

        return(val);
}

static struct oval_variable *oval_probe_variable_objgetvar(struct oval_object *obj)
{
        struct oval_entity  *ent;
        struct oval_object_content *con;
        struct oval_object_content_iterator *cit;
	struct oval_variable *var;
        char *ent_name;

	var = NULL;
        cit = oval_object_get_object_contents(obj);

	while (oval_object_content_iterator_has_more(cit)) {
                con = oval_object_content_iterator_next(cit);

                if (oval_object_content_get_type(con) != OVAL_OBJECTCONTENT_ENTITY)
                        continue;

                ent = oval_object_content_get_entity(con);
                ent_name = oval_entity_get_name(ent);

                if (strcmp(ent_name, "var_ref") != 0)
                        continue;

                var = oval_entity_get_variable(ent);
                break;
        }

        oval_object_content_iterator_free(cit);

        return(var);
}

static struct oval_syschar *oval_probe_envvar_eval(struct oval_object *obj, struct oval_syschar_model *model)
{
        struct oval_syschar *sys;
        struct oval_value   *val;
        char *var_name, *var_value;
	SEXP_t *r0, *r1, *r2, *cobj;

        val = oval_object_getentval(obj, "name");

        if (val == NULL)
                return(NULL);

        if (oval_value_get_datatype(val) != OVAL_DATATYPE_STRING)
                return(NULL);

        var_name  = oval_value_get_text(val);
        var_value = getenv(var_name);
	cobj = probe_cobj_new(SYSCHAR_FLAG_UNKNOWN, NULL, NULL);

	if (var_value != NULL) {
		r0 = probe_item_creat("environmentvariable_item", NULL,
				      "name",  NULL, r1 = SEXP_string_new(var_name,  strlen(var_name)),
				      "value", NULL, r2 = SEXP_string_new(var_value, strlen(var_value)),
				      NULL);
		probe_cobj_add_item(cobj, r0);
		SEXP_vfree (r0, r1, r2, NULL);
	}

	probe_cobj_compute_flag(cobj);
	sys  = oval_sexp2sysch(cobj, model, obj);
	SEXP_vfree(cobj, NULL);

        return(sys);
}

int oval_probe_envvar_handler(oval_subtype_t type, void *ptr, int act, ...)
{
        int ret = 0;
        va_list ap;
        struct oval_syschar_model *model = (struct oval_syschar_model *)ptr;

        va_start(ap, act);

        switch(act) {
        case PROBE_HANDLER_ACT_EVAL:
        {
                struct oval_object   *obj = va_arg(ap, struct oval_object *);
                struct oval_syschar **sys = va_arg(ap, struct oval_syschar **);

                va_arg(ap, int);
                *sys = oval_probe_envvar_eval(obj, model);
                ret  = (*sys == NULL ? -1 : 0);
                break;
        }
        case PROBE_HANDLER_ACT_INIT:
        case PROBE_HANDLER_ACT_OPEN:
        case PROBE_HANDLER_ACT_CLOSE:
        case PROBE_HANDLER_ACT_FREE:
        case PROBE_HANDLER_ACT_RESET:
                break;
        default:
                errno = EINVAL;
                ret = -1;
        }

        va_end(ap);
        return(ret);
}

static struct oval_syschar *oval_probe_variable_eval(struct oval_object *obj, oval_probe_session_t *sess)
{
        struct oval_value    *val;
        struct oval_syschar  *sys;
        struct oval_value_iterator *vit;
        struct oval_variable *var;
        struct oval_syschar_model    *sys_model;
	oval_syschar_collection_flag_t flag = SYSCHAR_FLAG_ERROR;

	var = oval_probe_variable_objgetvar(obj);
        sys_model = oval_probe_session_getmodel(sess);

	if (var == NULL) {
		goto fail;
	}

        if (oval_probe_query_variable(sess, var) != 0)
		goto fail;

	flag = oval_variable_get_collection_flag(var);
	switch (flag) {
	case SYSCHAR_FLAG_COMPLETE:
	case SYSCHAR_FLAG_INCOMPLETE:
		break;
	default:
		goto fail;
	}

        vit = oval_variable_get_values(var);

	if (vit == NULL) {
		flag = SYSCHAR_FLAG_ERROR;
		goto fail;
	} else {
                SEXP_t *r0, *item, *cobj, *vrent, *val_sexp;
		char *var_ref;

                cobj = probe_cobj_new(SYSCHAR_FLAG_UNKNOWN, NULL, NULL);

                /* Create shared entity */
		var_ref = oval_variable_get_id(var);
                vrent = probe_ent_creat1("var_ref", NULL,
                                         r0 = SEXP_string_new(var_ref, strlen(var_ref)));
                SEXP_free(r0);

                while (oval_value_iterator_has_more(vit)) {
			oval_datatype_t dtype;

                        val = oval_value_iterator_next(vit);

			oval_value_cast(val, OVAL_DATATYPE_STRING);

			dtype = oval_value_get_datatype(val);
			val_sexp = oval_value_to_sexp(val, dtype);
			assume_d(val_sexp != NULL, -1);

			item = probe_item_creat("variable_item", NULL,
						"value", NULL, val_sexp,
						NULL);
			/* Add shared entity */
			SEXP_list_add(item, vrent);

			/* Add item to the item list */
			probe_cobj_add_item(cobj, item);
			SEXP_vfree(item, val_sexp, NULL);
                }

                oval_value_iterator_free(vit);
		probe_cobj_compute_flag(cobj);
                sys  = oval_sexp2sysch(cobj, sys_model, obj);
                SEXP_vfree(cobj, vrent, NULL);
        }

        return(sys);
 fail:
	sys = oval_syschar_new(sys_model, obj);
	oval_syschar_set_flag(sys, flag);

	return(sys);
}

int oval_probe_var_handler(oval_subtype_t type, void *ptr, int act, ...)
{
        int ret = 0;
        va_list ap;
        oval_probe_session_t *sess = (oval_probe_session_t *)ptr;

        va_start(ap, act);

        switch(act) {
        case PROBE_HANDLER_ACT_EVAL:
        {
                struct oval_object   *obj = va_arg(ap, struct oval_object *);
                struct oval_syschar **sys = va_arg(ap, struct oval_syschar **);

                va_arg(ap, int);
                *sys = oval_probe_variable_eval(obj, sess);
                ret  = (*sys == NULL ? -1 : 0);
                break;
        }
        case PROBE_HANDLER_ACT_INIT:
        case PROBE_HANDLER_ACT_OPEN:
        case PROBE_HANDLER_ACT_CLOSE:
        case PROBE_HANDLER_ACT_FREE:
        case PROBE_HANDLER_ACT_RESET:
                break;
        default:
                errno = EINVAL;
                ret = -1;
        }

        va_end(ap);
        return(ret);
}
