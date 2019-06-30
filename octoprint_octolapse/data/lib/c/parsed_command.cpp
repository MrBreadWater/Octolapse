////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Octolapse - A plugin for OctoPrint used for making stabilized timelapse videos.
// Copyright(C) 2019  Brad Hochgesang
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.If not, see the following :
// https ://github.com/FormerLurker/Octolapse/blob/master/LICENSE
//
// You can contact the author either through the git - hub repository, or at the
// following email address : FormerLurker@pm.me
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "parsed_command.h"
#include "python_helpers.h"
#include "logging.h"
#include <sstream>
parsed_command::parsed_command()
{
	cmd_ = "";
	gcode_ = "";
}
parsed_command::parsed_command(parsed_command & source)
{
	cmd_ = source.cmd_;
	gcode_ = source.gcode_;
	for (unsigned int index = 0; index < source.parameters_.size(); index++)
		parameters_.push_back(new parsed_command_parameter(*source.parameters_[index]));
}

parsed_command::~parsed_command()
{
	for (std::vector< parsed_command_parameter * >::iterator it = parameters_.begin(); it != parameters_.end(); ++it)
	{
		delete (*it);
	}
	parameters_.clear();
}
void parsed_command::clear()
{
	cmd_ = "";
	gcode_ = "";
	for (std::vector< parsed_command_parameter * >::iterator it = parameters_.begin(); it != parameters_.end(); ++it)
	{
		delete (*it);
	}
	parameters_.clear();
}
PyObject * parsed_command::to_py_object()
{
	PyObject *ret_val;
	PyObject * pyCommandName = PyUnicode_SafeFromString(cmd_.c_str());
	
	if (pyCommandName == NULL)
	{
		PyErr_Print();
		std::string message = "Unable to convert the parameter name to unicode: ";
		message += cmd_;
		octolapse_log(octolapse_log::GCODE_PARSER, octolapse_log::ERROR, message);
		PyErr_SetString(PyExc_ValueError, message.c_str());
		return NULL;
	}
	PyObject * pyGcode = PyUnicode_SafeFromString(gcode_.c_str());
	if (pyGcode == NULL)
	{
		PyErr_Print();
		std::string message = "Unable to convert the gcode to unicode: ";
		message += gcode_;
		octolapse_log(octolapse_log::GCODE_PARSER, octolapse_log::ERROR, message);
		PyErr_SetString(PyExc_ValueError, message.c_str());
		return NULL;
	}
	
	if (parameters_.empty())
	{
		ret_val = PyTuple_Pack(3, pyCommandName, Py_None, pyGcode);
		if (ret_val == NULL)
		{
			PyErr_Print();
			std::string message = "Unable to convert the parsed_command (no parameters) to a tuple.  Command: ";
			message += cmd_;
			message += " Gcode: ";
			message += gcode_;
			octolapse_log(octolapse_log::GCODE_PARSER, octolapse_log::ERROR, message);
			PyErr_SetString(PyExc_ValueError, message.c_str());
			return NULL;
		}
		// We will need to decref pyCommandName and pyGcode later
	}
	else
	{
		PyObject * pyParametersDict = PyDict_New();

		// Create the parameters dictionary
		if (pyParametersDict == NULL)
		{
			PyErr_Print();
			std::string message = "ParsedCommand.to_py_object: Unable to create the parameters dict.";
			octolapse_log(octolapse_log::GCODE_PARSER, octolapse_log::ERROR, message);
			PyErr_SetString(PyExc_ValueError, message.c_str());
			return NULL;
		}
		// Loop through our parameters vector and create and add PyDict items
		for (unsigned int index = 0; index < parameters_.size(); index++)
		{
			parsed_command_parameter* param = parameters_[index];
			PyObject * param_value = param->value_to_py_object();
			// Errors here will be handled by value_to_py_object, just return NULL
			if (param_value == NULL)
				return NULL;
			
			if (PyDict_SetItemString(pyParametersDict, param->name_.c_str(), param_value) != 0)
			{
				PyErr_Print();
				// Handle error here, display detailed message
				std::string message = "Unable to add the command parameter to the parameters dictionary.  Parameter Name: ";
				message += param->name_;
				message += " Value Type: ";
				message += param->value_type_;
				message += " Value: ";

				switch (param->value_type_)
				{
				case 'S':
					message += param->string_value_;
					break;
				case 'N':
					message += "None";
					break;
				case 'F':
				{
					std::ostringstream doubld_str;
					doubld_str << param->double_value_;
					message += doubld_str.str();
					message += param->string_value_;
				}
					break;
				case 'U':
				{
					std::ostringstream unsigned_strs;
					unsigned_strs << param->unsigned_long_value_;
					message += unsigned_strs.str();
					message += param->string_value_;
				}
					break;
				default:
					break;
				}
				PyErr_SetString(PyExc_ValueError, message.c_str());
				return NULL;
			}
			// Todo: evaluate the effects of this
			Py_DECREF(param_value);
			//std::cout << "param_value refcount = " << param_value->ob_refcnt << "\r\n";
		}

		ret_val = PyTuple_Pack(3, pyCommandName, pyParametersDict, pyGcode);
		if (ret_val == NULL)
		{
			PyErr_Print();
			std::string message = "Unable to convert the parsed_command (with parameters) to a tuple.  Command: ";
			message += cmd_;
			message += " Gcode: ";
			message += gcode_;
			octolapse_log(octolapse_log::GCODE_PARSER, octolapse_log::ERROR, message);
			PyErr_SetString(PyExc_ValueError, message.c_str());
			return NULL;
		}
		// PyTuple_Pack makes a reference of its own, decref pyParametersDict.  
		// We will need to decref pyCommandName and pyGcode later
		// Todo: evaluate the effects of this
		Py_DECREF(pyParametersDict);
		//std::cout << "pyParametersDict refcount = " << pyParametersDict->ob_refcnt << "\r\n";
	}
	// If we're here, we need to decref pyCommandName and pyGcode.
	// Todo: evaluate the effects of this
	Py_DECREF(pyCommandName);
	// Todo: evaluate the effects of this
	Py_DECREF(pyGcode);
	//std::cout << "pyCommandName refcount = " << pyCommandName->ob_refcnt << "\r\n";
	//std::cout << "pyGcode refcount = " << pyGcode->ob_refcnt << "\r\n";
	//std::cout << "ret_val refcount = " << ret_val->ob_refcnt << "\r\n";
	return ret_val;
}
