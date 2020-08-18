// David Wilson - Code Sample

#pragma once

#include "Include1.h"
#include "Include2.h"
#include "Include3.h"


//----------------------------------------------------------------------
// t_named_param
//----------------------------------------------------------------------

template<typename PARAM>
struct t_named_param
{
	t_named_param() = default;

	t_named_param(const t_string& name, const PARAM& param) :
		m_param(param),
		m_name(name)
	{ }

	PARAM& param()					{ return m_param; }
	const PARAM& param() const		{ return m_param; }
	t_string& name()				{ return m_name; }
	const t_string& name() const	{ return m_name; }

private:

	PARAM							m_param;
	t_string						m_name;
};


//----------------------------------------------------------------------
// t_param_traits: Parameter handling for standard features
//----------------------------------------------------------------------

template<typename FEAT>
struct t_param_traits
{
	using parameter_type = t_id;
	using named_parameter_type = t_named_param<parameter_type>;
	using listparam_type = t_id_list;
	using named_listparam_type = t_named_param<listparam_type>;

	static bool deferred(parameter_type param)
	{ return param == ID_INVALID; }

	static const FEAT* feature(const t_live_simulation& sim, const t_string& name)
	{ return t_project::t_table_traits<FEAT>::table(sim).find(name); }

	static const FEAT* feature(const t_live_simulation& sim, parameter_type id)
	{ return t_project::t_table_traits<FEAT>::table(sim).lookup(id); }

	static const t_char* to_name(const FEAT* feature)
	{ return feature->t_nameable_entry::name().c_str(); }

	static parameter_type to_parameter(const FEAT* feature)
	{ return feature->id(); }

	static t_string param_string(parameter_type param)
	{ return t_error(STR_ID_NAME, param); }

	static void add_all_features(std::vector<named_parameter_type>& parameters, const t_live_simulation& sim)
	{
		for (auto& feature : t_project::t_table_traits<FEAT>::table(sim))
			parameters.emplace_back(feature.second.name(), to_parameter(&feature.second));
	}

	static void add_feature(named_listparam_type& parameter, const FEAT& feature)
	{
		if (parameter.param().size() <= 1)
			parameter.name() = parameter.param().empty() ? feature.name() : t_string();
		parameter.param().emplace_back(to_parameter(&feature));
	}

	static void add_all_features(named_listparam_type& parameter, const t_live_simulation& sim)
	{
		for (auto& feature : t_project::t_table_traits<FEAT>::table(sim))
			add_feature(parameter, feature.second);
	}

	static void add_all_features(std::vector<named_listparam_type>& parameters, const t_live_simulation& sim)
	{
		for (auto& feature : t_project::t_table_traits<FEAT>::table(sim))
		{
			parameters.emplace_back();
			add_feature(parameters.back(), feature.second);
		}
	}
};


//----------------------------------------------------------------------
// t_param_traits<t_trip>: Parameter handling for trips
//----------------------------------------------------------------------

template<>
struct t_param_traits<t_trip>
{
	using parameter_type = t_string;
	using named_parameter_type = t_named_param<parameter_type>;
	using listparam_type = t_string_list;
	using named_listparam_type = t_named_param<listparam_type>;
	
	static bool deferred(parameter_type param)
	{ return param.empty(); }

	static const t_trip* feature(const t_live_simulation& sim, const t_string& name)
	{ return sim.get_operating_plan().find_trip(name); }

	static const t_char* to_name(const t_trip* trip)
	{ return trip->name().c_str(); }

	static const parameter_type& to_parameter(const t_trip* trip)
	{ return trip->name(); }

	static t_string param_string(parameter_type param)
	{ return param; }

	static void add_all_features(std::vector<named_parameter_type>& parameters, const t_live_simulation& sim)
	{
		for (auto&& trip : sim.get_operating_plan().enabled_trips())
			parameters.emplace_back(trip.name(), to_parameter(&trip));
	}

	static void add_feature(named_listparam_type& parameter, const t_trip& trip)
	{
		if (parameter.param().size() <= 1)
			parameter.name() = parameter.param().empty() ? trip.name() : t_string();
		parameter.param().emplace_back(to_parameter(&trip));
	}

	static void add_all_features(named_listparam_type& parameter, const t_live_simulation& sim)
	{
		for (auto&& trip : sim.get_operating_plan().enabled_trips())
			add_feature(parameter, trip);
	}

	static void add_all_features(std::vector<named_listparam_type>& parameters, const t_live_simulation& sim)
	{
		for (auto&& trip : sim.get_operating_plan().enabled_trips())
		{
			parameters.emplace_back();
			add_feature(parameters.back(), trip);
		}
	}
};


//----------------------------------------------------------------------
// get_item_parameters
//----------------------------------------------------------------------

template<typename FEAT>
std::vector<typename t_param_traits<FEAT>::named_parameter_type> get_item_parameters(
	const t_batch_query_param& query_param,
	const typename t_param_traits<FEAT>::parameter_type& config_parameter,
	const t_live_simulation& sim
)
{
	// Parameter list
	using traits = t_param_traits<FEAT>;
	std::vector<traits::named_parameter_type> parameters;

	// Depending on parameter type
	switch (query_param.type())
	{
	// Multiple reports using each batch parameter
	case t_batch_query_param::t_type::EXPLICIT:

		// For each batch query parameter
		for (auto& name : query_param.m_values)
		{
			// If feature not found in simulation, choke
			auto feature = traits::feature(sim, name);
			if (!feature)
				throw t_log_event(BA022, NO_LINE_NUMBER, name);

			// Add feature to parameters
			parameters.emplace_back(traits::to_name(feature), traits::to_parameter(feature));
		}
		break;

	// Single report using configuration parameter
	case t_batch_query_param::t_type::CONFIG:

		// If configuration parameter deferred, choke
		if (traits::deferred(config_parameter))
			throw t_log_event(BA017, NO_LINE_NUMBER);
		{
			// If feature not found in simulation, choke
			auto feature = traits::feature(sim, config_parameter);
			if (!feature)
				throw t_log_event(BA018, NO_LINE_NUMBER, traits::param_string(config_parameter));

			// Add configuration parameter to list
			parameters.emplace_back(traits::to_name(feature), config_parameter);
		}
		break;

	// Multiple reports, one per simulation feature
	case t_batch_query_param::t_type::ALL:

		// Add all simulation feature parameters to list
		traits::add_all_features(parameters, sim);
		break;

	// Single report, all simulation features
	case t_batch_query_param::t_type::SINGLE:

		// Choke
		throw t_log_event(BA020, NO_LINE_NUMBER);
	}

	return parameters;
}


//----------------------------------------------------------------------
// get_list_parameters
//----------------------------------------------------------------------

template<typename FEAT>
std::vector<typename t_param_traits<FEAT>::named_listparam_type> get_list_parameters(
	const t_batch_query_param& query_param,
	const typename t_param_traits<FEAT>::listparam_type& config_listparam,
	bool config_all_parameters,
	const t_live_simulation& sim
)
{
	// Parameter list
	using traits = t_param_traits<FEAT>;
	traits::named_listparam_type parameter;
	std::vector<traits::named_listparam_type> parameters;

	// Depending on parameter type
	switch (query_param.type())
	{
	// Single report using all batch parameters
	case t_batch_query_param::t_type::EXPLICIT:

		// For each batch query parameter
		for (auto& name : query_param.m_values)
		{
			// If parameter not found in simulation, choke
			auto feature = traits::feature(sim, name);
			if (!feature)
				throw t_log_event(BA022, NO_LINE_NUMBER, name);

			// Add item to parameter
			traits::add_feature(parameter, *feature);
		}

		// Add parameter to list
		parameters.emplace_back(parameter);
		break;
		
	// Single report using configuration parameter
	case t_batch_query_param::t_type::CONFIG:

		// If "all parameters" set for configuration
		if (config_all_parameters)

			// Add all simulation features to parameter
			traits::add_all_features(parameter, sim);

		// Else using configuration parameter list
		else
		{
			// For each configuration parameter
			for (auto& config_parameter : config_listparam)
			{
				// If feature not found in simulation, choke
				auto feature = traits::feature(sim, config_parameter);
				if (!feature)
					throw t_log_event(BA018, NO_LINE_NUMBER, traits::param_string(config_parameter));

				// Add feature to parameter
				traits::add_feature(parameter, *feature);
			}
		}
		
		// Set list parameter name to feature name for singleton list
		parameter.name() =
			config_listparam.size() == 1
				? traits::to_name(traits::feature(sim, config_listparam.front()))
				: t_string();

		// Add parameter to list
		parameters.emplace_back(parameter);
		break;

	// Multiple reports, one per simulation feature
	case t_batch_query_param::t_type::ALL:

		// Add singleton list parameter per simulation feature
		traits::add_all_features(parameters, sim);
		break;

	// Single report, all simulation features
	case t_batch_query_param::t_type::SINGLE:

		// Add list parameter with all simulation features
		traits::add_all_features(parameter, sim);
		parameters.emplace_back(parameter);
		break;
	}

	return parameters;
}

