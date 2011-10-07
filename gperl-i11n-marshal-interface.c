static gpointer
instance_sv_to_pointer (GICallableInfo *info, SV *sv)
{
	// We do *not* own container.
	GIBaseInfo *container = g_base_info_get_container (info);
	GIInfoType info_type = g_base_info_get_type (container);
	gpointer pointer = NULL;

	/* FIXME: Much of this code is duplicated in sv_to_interface. */

	dwarn ("  instance_sv_to_pointer: container name: %s, info type: %d\n",
	       g_base_info_get_name (container),
	       info_type);

	switch (info_type) {
	    case GI_INFO_TYPE_OBJECT:
	    case GI_INFO_TYPE_INTERFACE:
		pointer = gperl_get_object (sv);
		dwarn ("    -> object pointer: %p\n", pointer);
		break;

	    case GI_INFO_TYPE_BOXED:
	    case GI_INFO_TYPE_STRUCT:
            case GI_INFO_TYPE_UNION:
	    {
		GType type = g_registered_type_info_get_g_type (
			       (GIRegisteredTypeInfo *) container);
		if (!type || type == G_TYPE_NONE) {
			dwarn ("    unboxed type\n");
			pointer = sv_to_struct (GI_TRANSFER_NOTHING,
			                        container,
			                        info_type,
			                        sv);
		} else {
			dwarn ("    boxed type: %s (%d)\n",
			       g_type_name (type), type);
			pointer = gperl_get_boxed_check (sv, type);
		}
		dwarn ("    -> boxed pointer: %p\n", pointer);
		break;
	    }

	    default:
		ccroak ("instance_sv_to_pointer: Don't know how to handle info type %d", info_type);
	}

	return pointer;
}

static void
sv_to_interface (GIArgInfo * arg_info,
                 GITypeInfo * type_info,
                 SV * sv,
                 GIArgument * arg,
                 GPerlI11nInvocationInfo * invocation_info)
{
	GIBaseInfo *interface;
	GIInfoType info_type;

	interface = g_type_info_get_interface (type_info);
	if (!interface)
		ccroak ("Could not convert sv %p to pointer", sv);
	info_type = g_base_info_get_type (interface);

	dwarn ("    interface %p (%s) of type %d\n",
	       interface, g_base_info_get_name (interface), info_type);

	switch (info_type) {
	    case GI_INFO_TYPE_OBJECT:
	    case GI_INFO_TYPE_INTERFACE:
		/* FIXME: Check transfer setting. */
		arg->v_pointer = gperl_get_object (sv);
		break;

	    case GI_INFO_TYPE_UNION:
	    case GI_INFO_TYPE_STRUCT:
	    case GI_INFO_TYPE_BOXED:
	    {
		/* FIXME: What about pass-by-value here? */
		GType type = g_registered_type_info_get_g_type (
		               (GIRegisteredTypeInfo *) interface);
		if (!type || type == G_TYPE_NONE) {
			GITransfer transfer = arg_info
				? g_arg_info_get_ownership_transfer (arg_info)
				: GI_TRANSFER_NOTHING;
			dwarn ("    unboxed type\n");
			arg->v_pointer = sv_to_struct (transfer,
			                               interface,
			                               info_type,
			                               sv);
		} else if (type == G_TYPE_CLOSURE) {
			/* FIXME: User cannot supply user data. */
			dwarn ("    closure type\n");
			arg->v_pointer = gperl_closure_new (sv, NULL, FALSE);
		} else if (type == G_TYPE_VALUE) {
			dwarn ("    value type\n");
			arg->v_pointer = SvGValueWrapper (sv);
			if (!arg->v_pointer)
				ccroak ("Cannot convert arbitrary SV to GValue");
		} else {
			dwarn ("    boxed type: %s (%d)\n",
			       g_type_name (type), type);
			/* FIXME: Check transfer setting. */
			arg->v_pointer = gperl_get_boxed_check (sv, type);
		}
		break;
	    }

	    case GI_INFO_TYPE_ENUM:
	    {
		GType type = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) interface);
		/* FIXME: Check storage type? */
		arg->v_long = gperl_convert_enum (type, sv);
		break;
	    }

	    case GI_INFO_TYPE_FLAGS:
	    {
		GType type = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) interface);
		/* FIXME: Check storage type? */
		arg->v_long = gperl_convert_flags (type, sv);
		break;
	    }

	    case GI_INFO_TYPE_CALLBACK:
		arg->v_pointer = sv_to_callback (arg_info, type_info, sv,
		                                 invocation_info);
		break;

	    default:
		ccroak ("sv_to_interface: Don't know how to handle info type %d", info_type);
	}

	g_base_info_unref ((GIBaseInfo *) interface);
}

static SV *
interface_to_sv (GITypeInfo* info, GIArgument *arg, gboolean own)
{
	GIBaseInfo *interface;
	GIInfoType info_type;
	SV *sv = NULL;

	dwarn ("  interface_to_sv: arg %p, info %p\n",
	       arg, info);

	interface = g_type_info_get_interface (info);
	if (!interface)
		ccroak ("Could not convert arg %p to SV", arg);
	info_type = g_base_info_get_type (interface);
	dwarn ("    info type: %d (%s)\n", info_type, g_info_type_to_string (info_type));

	switch (info_type) {
	    case GI_INFO_TYPE_OBJECT:
	    case GI_INFO_TYPE_INTERFACE:
		sv = gperl_new_object (arg->v_pointer, own);
		break;

	    case GI_INFO_TYPE_UNION:
	    case GI_INFO_TYPE_STRUCT:
	    case GI_INFO_TYPE_BOXED:
	    {
		/* FIXME: What about pass-by-value here? */
		GType type;
		type = g_registered_type_info_get_g_type (
		               (GIRegisteredTypeInfo *) interface);
		if (!type || type == G_TYPE_NONE) {
			dwarn ("    unboxed type\n");
			sv = struct_to_sv (interface, info_type, arg->v_pointer, own);
		} else if (type == G_TYPE_VALUE) {
			dwarn ("    value type\n");
			sv = gperl_sv_from_value (arg->v_pointer);
			/* FIXME: Check 'own'. */
		} else {
			dwarn ("    boxed type: %d (%s)\n",
			       type, g_type_name (type));
			sv = gperl_new_boxed (arg->v_pointer, type, own);
		}
		break;
	    }

	    case GI_INFO_TYPE_ENUM:
	    {
		GType type = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) interface);
		/* FIXME: Is it right to just use v_long here? */
		sv = gperl_convert_back_enum (type, arg->v_long);
		break;
	    }

	    case GI_INFO_TYPE_FLAGS:
	    {
		GType type = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) interface);
		/* FIXME: Is it right to just use v_long here? */
		sv = gperl_convert_back_flags (type, arg->v_long);
		break;
	    }

	    default:
		ccroak ("interface_to_sv: Don't know how to handle info type %d", info_type);
	}

	g_base_info_unref ((GIBaseInfo *) interface);

	return sv;
}
