/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2022
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

// Temporarily "disable" debug to include Python.
// This is done, so that the Python debug libraries are not needed, even when building
// this module in debug mode.
#ifdef _DEBUG
#	define TMP_DEBUG
#	undef _DEBUG
#endif
#include <Python.h>
#ifdef TMP_DEBUG
#	define _DEBUG
#	undef TMP_DEBUG
#endif

#include "vasingletondoc.hpp"

// All Python to VA methods. Also pulls in g_pVAError (Python error trace instance)
#include "vasingletonmethods.hpp"

// VA methods that will appear in Python if they are added to the following table
// It's corresponding C++ functions are implemented here: vasingletonmethods.hpp

static struct PyMethodDef va_methods[] = {
	{ "connect", (PyCFunction)connect, METH_VARARGS | METH_KEYWORDS, connect_doc },
	{ "disconnect", (PyCFunction)disconnect, METH_NOARGS, no_doc },
	{ "is_connected", (PyCFunction)is_connected, METH_NOARGS, no_doc },
	{ "reset", (PyCFunction)reset, METH_NOARGS, no_doc },

	{ "get_version", (PyCFunction)get_version, METH_NOARGS, no_doc },
	{ "get_modules", (PyCFunction)get_modules, METH_NOARGS, no_doc },
	{ "call_module", (PyCFunction)call_module, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_search_paths", (PyCFunction)get_search_paths, METH_NOARGS, no_doc },
	{ "add_search_path", (PyCFunction)add_search_path, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "create_directivity_from_file", (PyCFunction)create_directivity_from_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "delete_directivity", (PyCFunction)delete_directivity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_directivity_info", (PyCFunction)get_directivity_info, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_directivity_infos", (PyCFunction)get_directivity_infos, METH_NOARGS, no_doc },

	{ "create_signal_source_buffer_from_file", (PyCFunction)create_signal_source_buffer_from_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_prototype_from_parameters", (PyCFunction)create_signal_source_prototype_from_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_text_to_speech", (PyCFunction)create_signal_source_text_to_speech, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_sequencer", (PyCFunction)create_signal_source_sequencer, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_network_stream", (PyCFunction)create_signal_source_network_stream, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_engine", (PyCFunction)create_signal_source_engine, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_signal_source_machine", (PyCFunction)create_signal_source_machine, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "delete_signal_source", (PyCFunction)delete_signal_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_info", (PyCFunction)get_signal_source_info, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_infos", (PyCFunction)get_signal_source_infos, METH_NOARGS, no_doc },
	{ "get_signal_source_buffer_playback_state", (PyCFunction)get_signal_source_buffer_playback_state, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_buffer_playback_state_str", (PyCFunction)get_signal_source_buffer_playback_state_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_buffer_playback_action", (PyCFunction)set_signal_source_buffer_playback_action, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_buffer_playback_action_str", (PyCFunction)set_signal_source_buffer_playback_action_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_buffer_playback_position", (PyCFunction)set_signal_source_buffer_playback_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_buffer_looping", (PyCFunction)get_signal_source_buffer_looping, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_buffer_looping", (PyCFunction)set_signal_source_buffer_looping, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_start_machine", (PyCFunction)set_signal_source_machine_start_machine, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_halt_machine", (PyCFunction)set_signal_source_machine_halt_machine, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_machine_state_str", (PyCFunction)get_signal_source_machine_state_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_speed", (PyCFunction)set_signal_source_machine_speed, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_machine_speed", (PyCFunction)get_signal_source_machine_speed, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_start_file", (PyCFunction)set_signal_source_machine_start_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_idle_file", (PyCFunction)set_signal_source_machine_idle_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_machine_stop_file", (PyCFunction)set_signal_source_machine_stop_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_signal_source_parameters", (PyCFunction)set_signal_source_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_signal_source_parameters", (PyCFunction)get_signal_source_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_sound_source_ids", (PyCFunction)get_sound_source_ids, METH_NOARGS, no_doc },
	{ "create_sound_source", (PyCFunction)create_sound_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_sound_source_explicit_renderer", (PyCFunction)create_sound_source_explicit_renderer, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "delete_sound_source", (PyCFunction)delete_sound_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_enabled", (PyCFunction)set_sound_source_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_enabled", (PyCFunction)get_sound_source_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_name", (PyCFunction)get_sound_source_name, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_enabled", (PyCFunction)set_sound_source_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_signal_source", (PyCFunction)get_sound_source_signal_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_signal_source", (PyCFunction)set_sound_source_signal_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "remove_sound_source_signal_source", (PyCFunction)remove_sound_source_signal_source, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_auralization_mode", (PyCFunction)get_sound_source_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_auralization_mode", (PyCFunction)set_sound_source_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_parameters", (PyCFunction)set_sound_source_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_parameters", (PyCFunction)get_sound_source_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_directivity", (PyCFunction)get_sound_source_directivity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_directivity", (PyCFunction)set_sound_source_directivity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_sound_power", (PyCFunction)get_sound_source_sound_power, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_sound_power", (PyCFunction)set_sound_source_sound_power, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_muted", (PyCFunction)get_sound_source_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_muted", (PyCFunction)set_sound_source_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_position", (PyCFunction)get_sound_source_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_position", (PyCFunction)set_sound_source_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_orientation_vu", (PyCFunction)get_sound_source_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_orientation_vu", (PyCFunction)set_sound_source_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_source_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_source_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_sound_receiver_ids", (PyCFunction)get_sound_receiver_ids, METH_NOARGS, no_doc },
	{ "create_sound_receiver", (PyCFunction)create_sound_receiver, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_sound_receiver_explicit_renderer", (PyCFunction)create_sound_receiver_explicit_renderer, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "delete_sound_receiver", (PyCFunction)delete_sound_receiver, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_enabled", (PyCFunction)set_sound_receiver_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_enabled", (PyCFunction)get_sound_receiver_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_name", (PyCFunction)get_sound_receiver_name, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_enabled", (PyCFunction)set_sound_receiver_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_auralization_mode", (PyCFunction)get_sound_receiver_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_auralization_mode", (PyCFunction)set_sound_receiver_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_parameters", (PyCFunction)set_sound_receiver_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_parameters", (PyCFunction)get_sound_receiver_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_directivity", (PyCFunction)get_sound_receiver_directivity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_directivity", (PyCFunction)set_sound_receiver_directivity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_muted", (PyCFunction)get_sound_receiver_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_muted", (PyCFunction)set_sound_receiver_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_position", (PyCFunction)get_sound_receiver_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_position", (PyCFunction)set_sound_receiver_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_orientation_vu", (PyCFunction)get_sound_receiver_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_orientation_vu", (PyCFunction)set_sound_receiver_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_real_world_position", (PyCFunction)get_sound_receiver_real_world_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_real_world_position", (PyCFunction)set_sound_receiver_real_world_position, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_real_world_orientation_vu", (PyCFunction)get_sound_receiver_real_world_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_real_world_orientation_vu", (PyCFunction)set_sound_receiver_real_world_orientation_vu, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_receiver_real_world_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_receiver_real_world_orientation_q", (PyCFunction)not_implemented, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_sound_portal_ids", (PyCFunction)get_sound_portal_ids, METH_NOARGS, no_doc },
	{ "get_sound_portal_name", (PyCFunction)get_sound_portal_name, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_portal_name", (PyCFunction)set_sound_portal_name, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_sound_portal_enabled", (PyCFunction)get_sound_portal_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_sound_portal_enabled", (PyCFunction)set_sound_portal_enabled, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_homogeneous_medium_sound_speed", (PyCFunction)get_homogeneous_medium_sound_speed, METH_NOARGS, no_doc },
	{ "set_homogeneous_medium_sound_speed", (PyCFunction)set_homogeneous_medium_sound_speed, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_homogeneous_medium_temperature", (PyCFunction)get_homogeneous_medium_temperature, METH_NOARGS, no_doc },
	{ "set_homogeneous_medium_temperature", (PyCFunction)set_homogeneous_medium_temperature, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_homogeneous_medium_static_pressure", (PyCFunction)get_homogeneous_medium_static_pressure, METH_NOARGS, no_doc },
	{ "set_homogeneous_medium_static_pressure", (PyCFunction)set_homogeneous_medium_static_pressure, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_homogeneous_medium_relative_humidity", (PyCFunction)get_homogeneous_medium_relative_humidity, METH_NOARGS, no_doc },
	{ "set_homogeneous_medium_relative_humidity", (PyCFunction)set_homogeneous_medium_relative_humidity, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_homogeneous_medium_shift_speed", (PyCFunction)get_homogeneous_medium_shift_speed, METH_NOARGS, no_doc },
	{ "set_homogeneous_medium_shift_speed", (PyCFunction)set_homogeneous_medium_shift_speed, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_homogeneous_medium_parameters", (PyCFunction)get_homogeneous_medium_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_homogeneous_medium_parameters", (PyCFunction)set_homogeneous_medium_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },


	{ "create_acoustic_material", (PyCFunction)create_acoustic_material, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_acoustic_material_from_file", (PyCFunction)create_acoustic_material_from_file, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "create_acoustic_material_from_parameters", (PyCFunction)create_acoustic_material_from_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_acoustic_material_infos", (PyCFunction)get_acoustic_material_infos, METH_NOARGS, no_doc },


	{ "get_rendering_modules", (PyCFunction)get_rendering_modules, METH_NOARGS, no_doc },
	{ "set_rendering_module_muted", (PyCFunction)set_rendering_module_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_rendering_module_muted", (PyCFunction)get_rendering_module_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_rendering_module_gain", (PyCFunction)set_rendering_module_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_rendering_module_gain", (PyCFunction)get_rendering_module_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_rendering_module_auralization_mode", (PyCFunction)get_rendering_module_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_rendering_module_auralization_mode", (PyCFunction)set_rendering_module_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_rendering_module_parameters", (PyCFunction)get_rendering_module_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_rendering_module_parameters", (PyCFunction)set_rendering_module_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_reproduction_modules", (PyCFunction)get_reproduction_modules, METH_NOARGS, no_doc },
	{ "set_reproduction_module_muted", (PyCFunction)set_reproduction_module_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_reproduction_module_muted", (PyCFunction)get_reproduction_module_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_reproduction_module_gain", (PyCFunction)set_reproduction_module_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_reproduction_module_gain", (PyCFunction)get_reproduction_module_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_reproduction_module_parameters", (PyCFunction)get_reproduction_module_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_reproduction_module_parameters", (PyCFunction)set_reproduction_module_parameters, METH_VARARGS | METH_KEYWORDS, no_doc },


	{ "lock_update", (PyCFunction)lock_update, METH_NOARGS, no_doc },
	{ "unlock_update", (PyCFunction)unlock_update, METH_NOARGS, no_doc },
	{ "get_update_locked", (PyCFunction)get_update_locked, METH_NOARGS, no_doc },

	{ "get_input_gain", (PyCFunction)get_input_gain, METH_NOARGS, no_doc },
	{ "set_input_gain", (PyCFunction)set_input_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_input_muted", (PyCFunction)get_input_muted, METH_NOARGS, no_doc },
	{ "set_input_muted", (PyCFunction)set_input_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_output_gain", (PyCFunction)get_output_gain, METH_NOARGS, no_doc },
	{ "set_output_gain", (PyCFunction)set_output_gain, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_output_muted", (PyCFunction)get_output_muted, METH_NOARGS, no_doc },
	{ "set_output_muted", (PyCFunction)set_output_muted, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_global_auralization_mode", (PyCFunction)get_global_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "set_global_auralization_mode", (PyCFunction)set_global_auralization_mode, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_core_clock", (PyCFunction)get_core_clock, METH_NOARGS, no_doc },
	{ "set_core_clock", (PyCFunction)set_core_clock, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "substitute_macro", (PyCFunction)substitute_macro, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "find_file_path", (PyCFunction)find_file_path, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ "get_core_configuration", (PyCFunction)get_core_configuration, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_hardware_configuration", (PyCFunction)get_hardware_configuration, METH_NOARGS, no_doc },
	{ "get_file_list", (PyCFunction)get_file_list, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_log_level_str", (PyCFunction)get_log_level_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "parse_auralization_mode_str", (PyCFunction)parse_auralization_mode_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_auralization_mode_str", (PyCFunction)get_auralization_mode_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_volume_str_decibel", (PyCFunction)get_volume_str_decibel, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "parse_playback_state_str", (PyCFunction)parse_playback_state_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_playback_state_str", (PyCFunction)get_playback_state_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "parse_playback_action_str", (PyCFunction)parse_playback_action_str, METH_VARARGS | METH_KEYWORDS, no_doc },
	{ "get_playback_action_str", (PyCFunction)get_playback_action_str, METH_VARARGS | METH_KEYWORDS, no_doc },

	{ NULL, NULL }
};

static struct PyModuleDef vamoduledef = { PyModuleDef_HEAD_INIT, "va", module_doc, -1, va_methods, NULL, NULL, NULL, NULL };

PyMODINIT_FUNC PyInit_va( void )
{
	PyObject* pModule = PyModule_Create( &vamoduledef );
	g_pVAError        = PyErr_NewException( "va.error", NULL, NULL );
	Py_INCREF( g_pVAError );

	// PyAdd
	return pModule;
}
