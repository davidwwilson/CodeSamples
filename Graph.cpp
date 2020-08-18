// Sample code - David Wilson

#include "Include1.h"
#include "Include2.h"


// ------------------------------------------------------------------------
// t_rms_bar_base
// ------------------------------------------------------------------------

t_rms_bar_base::t_rms_bar_base(const t_char* name, int color, bool use_right_axis) :
	m_min_value(0),
	m_max_value(0),
	m_use_right_axis(use_right_axis),
	m_color(make_graph_color(color)),
	m_name(name)
{ }

void t_rms_bar_base::clear_data()
{
	m_data.clear();
	m_min_value = 0;
	m_max_value = 0;
}

void t_rms_bar_base::add_datum(double datum)
{
	m_data.push_back(datum);
	at_most(m_min_value, datum);
	at_least(m_max_value, datum);
}

void t_rms_bar_base::rebuild(const t_component_info_set& infos)
{
	for (auto& info : infos)
	{
		auto& calc = m_calculators.emplace(info.component().id(), calculator(infos.time_step())).first->second;
		calc->rebuild();
		calc->restart(info.comp_buffer());
		add_datum(to_user(calc->initial_value()));
	}
}

void t_rms_bar_base::restart(const t_component_info_set& infos)
{
	for (auto& info : infos)
		m_calculators[info.component().id()]->restart(info.comp_buffer());
}

void t_rms_bar_base::update(const t_component_info_set& infos)
{
	for (auto& info : infos)
	{
		auto& calc = m_calculators[info.component().id()];
		calc->update();
		add_datum(to_user(calc->has_value() ? calc->value() : 0.0));
	}
}

void t_rms_bar_base::add_to_layer(BarLayer* layer, int vi_lo, int vi_hi) const
{
	DataSet* dataset = layer->addDataSet(DoubleArray(m_data.data() + vi_lo, vi_hi - vi_lo), color(), name());
	if (m_use_right_axis)
		dataset->setUseYAxis2();
}

void t_rms_bar_base::add_to_legend(t_legend& legend) const
{
	legend.add_key(name(), color());
}


// ------------------------------------------------------------------------
// t_component_bar_set
// ------------------------------------------------------------------------

void t_component_bar_set::initialize(int units)
{
	m_bars.clear();
	m_left_min			= 0;
	m_left_max			= 0;
	m_right_min			= 0;
	m_right_max			= 0;
	m_units				= units;
	m_vi_lo				= 0;
	m_vi_hi				= 0;
	m_use_left_axis		= false;
	m_use_right_axis	= false;
}

void t_component_bar_set::clear_data()
{
	m_left_min		= 0;
	m_right_min		= 0;
	m_left_max		= 0;
	m_right_max		= 0;
	for (auto& bar : m_bars)
		bar->clear_data();
}

void t_component_bar_set::set_zoom(const t_component_info_set& infos)
{
	m_vi_lo		= infos.vi_lo();
	m_vi_hi		= infos.vi_hi();
}

void t_component_bar_set::add_to_layer(BarLayer* layer)
{
	for (auto& bar : m_bars)
		bar.get()->add_to_layer(layer, m_vi_lo, m_vi_hi);
}

void t_component_bar_set::add_to_legend(t_legend & legend)
{
	for (auto& bar : m_bars)
		bar.get()->add_to_legend(legend);
}

void t_component_bar_set::update_axis_bounds()
{
	m_left_min	= 0;
	m_left_max	= 0;
	m_right_min	= 0;
	m_right_max	= 0;
	bool use_left_axis	= false;
	bool use_right_axis	= false;
	for (const auto& bar : m_bars)
	{
		if (bar->use_right_axis())
		{
			at_most(m_right_min, bar->min_value());
			at_least(m_right_max, bar->max_value());
			use_right_axis = true;
		}
		else
		{
			at_most(m_left_min, bar->min_value());
			at_least(m_left_max, bar->max_value());
			use_left_axis = true;
		}
	}
	auto axis_min	= std::min(m_left_min, m_right_min);
	auto axis_max	= std::max(m_left_max, m_right_max);

	// If bars all positive, extend axes upward to minimum range
	constexpr double min_range = 1;
	if (axis_min == 0)
	{
		at_least(m_left_max, m_left_min + min_range);
		at_least(m_right_max, m_right_min + min_range);		
	}

	// Else if bars all negative, extend axes downward to minimum range
	else if (axis_max == 0)
	{
		// Extend axes downward to minimum range
		at_most(m_left_min, m_left_max - min_range);
		at_most(m_right_min, m_right_max - min_range);
	}

	// Else bars both positive and negative
	else
	{
		// Extend left and right axes minimally so zero aligns
		auto scale = axis_max/axis_min;
		if (m_left_max <= m_left_min*scale)
			m_left_max = m_left_min*scale;
		else
			m_left_min = m_left_max/scale;
		if (m_right_max <= m_right_min*scale)
			m_right_max = m_right_min*scale;
		else
			m_right_min = m_right_max/scale;

		// Extend left axis outward to minimum range
		if (auto left_range = m_left_max - m_left_min; left_range == 0)
		{
			m_left_min	= -0.5*min_range;
			m_right_min	=  0.5*min_range;
		}
		else if (left_range < min_range)
		{
			m_left_min	*= min_range/left_range;
			m_left_max	*= min_range/left_range;
		}

		// Extend right axis outward to minimum range
		if (auto right_range = m_right_max - m_right_min; right_range == 0)
		{
			m_right_min	= -0.5*min_range;
			m_right_min	=  0.5*min_range;
		}
		else if (right_range < min_range)
		{
			m_right_min	*= min_range/right_range;
			m_right_max	*= min_range/right_range;
		}
	}
}


// ------------------------------------------------------------------------
// t_rms_graph
// ------------------------------------------------------------------------

t_rms_graph::t_rms_graph(const t_simulation& project, const t_live_simulation& simulation) :
	inherited(project, simulation),
	m_component(nullptr)
{
	//m_plot_color_manager.rebuild();
	m_plot_color_manager.set_plot_color(m_plot_color_left,	COLOR_CURRENT);
	m_plot_color_manager.set_plot_color(m_plot_color_right,	COLOR_CURRENT);
}

void t_rms_graph::init_legend(int left, int top, int width)
{
	m_legend.initialize(left, top, width);
	m_component_bar_set.add_to_legend(m_legend);
}

void t_rms_graph::display_tool_tips(BaseChart * chart)
{
	// Currently, RMS graphs show only current, power and energy.
	// These quantities all have report precision 3.
	chartviewer().setImageMap(chart->getHTMLImageMap("clickable", "", "title='{value|3}'"));
}

bool t_rms_graph::can_zoom_in(int zoomDirection) const
{
	if (zoomDirection == Chart::DirectionVertical) return false;
	return !m_component_info_set.zoomed_in();
}

bool t_rms_graph::can_zoom_out(int zoomDirection) const
{
	if (zoomDirection == Chart::DirectionVertical) return false;
	return !m_component_info_set.zoomed_out();
}

bool t_rms_graph::zoom_at(int zoomDirection, int x, int y, double zoomRatio)
{
	if (zoomDirection == Chart::DirectionVertical) return false;

	// Get plot area dimensions
	int left	= m_plot_bounds.left;
	int width	= m_plot_bounds.Width();

	// Zoom info set to zoom region
	m_component_info_set.zoom_at((x - left)/double(width), zoomRatio);
	return true;
}

bool t_rms_graph::zoom_to(int zoomDirection, int x1, int y1, int x2, int y2)
{
	if (zoomDirection == Chart::DirectionVertical) return false;

	// Get plot area dimensions
	int left	= m_plot_bounds.left;
	int width	= m_plot_bounds.Width();

	// Translate zoom bounds relative plot area
	x1 -= left;
	x2 -= left;

	// Clip zoom bounds to plot bounds
	at_least(x1, 0);
	at_most(x2, width);

	// If zoom bounds outside plot bounds, no zoom
	if (x1 >= x2 || y1 >= y2) return false;

	// Zoom info set to zoom region
	m_component_info_set.zoom_to(x1/double(width), x2/double(width));
	return true;
}

void t_rms_graph::start_drag(int x, int y)
{
	m_component_info_set.start_drag((x - m_plot_bounds.left)/double(m_plot_bounds.Width()));
}

bool t_rms_graph::drag_to(int scrollDirection, int deltaX, int deltaY)
{
	m_component_info_set.drag_to(deltaX/double(m_plot_bounds.Width()));
	return true;
}

t_chart_ptr t_rms_graph::get_chart(int page_width, int page_height, bool vector_graphics)
{
	// Lay out chart features
	int left_axis_width		= PAGE_MARGIN_LEFT + LEFT_AXIS_WIDTH;
	int right_axis_width	= PAGE_MARGIN_RIGHT + RIGHT_AXIS_WIDTH;
	int plot_width			= page_width - left_axis_width - right_axis_width;
	int header_height;
	int legend_top, legend_height;
	int chart_top, chart_height;

	bool show_header		= m_show_header;
	bool show_legend		= m_show_legend;

	for (;;)
	{
		at_least(plot_width, CHART_WIDTH_MIN);

		header_height = PAGE_MARGIN_TOP + (show_header ? m_header.height(page_width) + t_legend::MARGIN_TOP : 0);

		legend_top = header_height;
		legend_height = 0;
		if (show_legend)
		{
			init_legend(left_axis_width, legend_top, plot_width);
			legend_height = m_legend.height() + t_legend::MARGIN_BOTTOM;
		}

		chart_top = legend_top + legend_height;
		int name_axis_height = m_max_name_width + NAME_AXIS_WIDTH;
		chart_height = page_height - chart_top - name_axis_height - PAGE_MARGIN_BOTTOM;

		if (chart_height >= CHART_HEIGHT_MIN) break;
		if (show_header)				show_header = false;
		else if (show_legend)			show_legend = false;
		else {
			at_least(chart_height, CHART_HEIGHT_MIN);
			break;
		}
	}

	// Get scroll and zoom positions.
	double vp_top = chartviewer().getViewPortTop();
	double vp_height = chartviewer().getViewPortHeight();

	// Header and legend.
	shared_ptr<XYChart> chart_ptr(new XYChart(page_width, page_height));
	XYChart& chart(*chart_ptr);
	if (vector_graphics) chart.enableVectorOutput();
    chart.setClipping();

	auto hgrid_color = [this](int color) -> int { return m_show_horizontal_marks ? color : Chart::Transparent; };
	int vgrid_color = m_show_vertical_marks ? MAIN_MARK_COLOR : Chart::Transparent;

	m_plot_bounds = CRect(left_axis_width, chart_top, left_axis_width + plot_width, chart_top + chart_height);
	PlotArea* plot_area = chart.setPlotArea(left_axis_width, chart_top, plot_width, chart_height,
		Chart::Transparent, -1, -1, hgrid_color(MAIN_MARK_COLOR), vgrid_color);

	if (show_header)
		m_header.plot(chart);

	if (show_legend)
		m_legend.plot(chart);

	m_component_bar_set.set_zoom(m_component_info_set);

	Axis* left_axis			= chart.yAxis();
	Axis* right_axis		= chart.yAxis2();
	bool use_left_axis		= m_component_bar_set.use_left_axis();
	bool use_right_axis		= m_component_bar_set.use_right_axis();

	bool show_axes			= !m_component_info_set.empty() && (
		!dataset().empty()
		|| m_component_bar_set.left_max() > 0
		|| m_component_bar_set.right_max() > 0
	);

	if (show_axes)
	{
		if (use_left_axis)
		{
			double left_min			= m_component_bar_set.left_min();
			double left_max			= m_component_bar_set.left_max();
			double left_range		= left_max - left_min;
			double axis_max			= left_max - left_range * vp_top;
			double axis_min			= axis_max - left_range * vp_height;
	
			left_axis->setLabelStyle(s_axis_label_font.file(), s_axis_label_font.size(), m_plot_color_left->m_axis_color);
			left_axis->setTitle(m_label_left, s_axis_title_font.file(), s_axis_title_font.size());
			left_axis->setLinearScale(axis_min, axis_max);
			left_axis->setRounding(false, false);
			left_axis->setTickDensity(20, 3);
			left_axis->setAutoScale(0.01, 0, 0);
			plot_area->setGridColor(hgrid_color(m_plot_color_left->m_mark_color), vgrid_color, Chart::Transparent, Chart::Transparent);
		}

		if (use_right_axis)
		{
			double right_min		= m_component_bar_set.right_min();
			double right_max		= m_component_bar_set.right_max();
			double right_range		= right_max - right_min;
			double axis_max			= right_max - right_range * vp_top;
			double axis_min			= axis_max - right_range * vp_height;
	
			right_axis->setLabelStyle(s_axis_label_font.file(), s_axis_label_font.size(), m_plot_color_right->m_axis_color);
			right_axis->setTitle(m_label_right, s_axis_title_font.file(), s_axis_title_font.size())->setFontAngle(TOP_DOWN_LABEL_ANGLE);
			right_axis->setLinearScale(axis_min, axis_max);
			right_axis->setRounding(false, false);
			right_axis->setTickDensity(20, 3);
			right_axis->setAutoScale(0.01, 0, 0);
		}

		if (!use_left_axis)
		{
			left_axis->copyAxis(right_axis);
			left_axis->setTitle(m_label_right, s_axis_title_font.file(), s_axis_title_font.size());
			left_axis->setLabelStyle(s_axis_label_font.file(), s_axis_label_font.size(), m_plot_color_right->m_axis_color);
			plot_area->setGridColor(hgrid_color(m_plot_color_right->m_mark_color), vgrid_color, Chart::Transparent, Chart::Transparent);
		}

		if (!use_right_axis)
		{
			right_axis->copyAxis(left_axis);
			right_axis->setTitle(m_label_left, s_axis_title_font.file(), s_axis_title_font.size())->setFontAngle(TOP_DOWN_LABEL_ANGLE);
			right_axis->setLabelStyle(s_axis_label_font.file(), s_axis_label_font.size(), m_plot_color_left->m_axis_color);
		}
	}

	else
	{
		left_axis->setLabelFormat("");
		left_axis->setTickColor(Chart::Transparent);
		right_axis->setLabelFormat("");
		right_axis->setTickColor(Chart::Transparent);
		plot_area->setGridColor(Chart::Transparent, vgrid_color, Chart::Transparent, Chart::Transparent);
	}

	auto zoomed = m_component_info_set.zoomed();
	auto name_count = ::size(zoomed);

	if (zoomed)
	{
		Axis* name_axis = chart.xAxis();
		name_axis->setTitle(m_component_label, s_axis_title_font.file(), s_axis_title_font.size());
		name_axis->setTitlePos(Chart::TopCenter, - m_max_name_width - 16);
		name_axis->setLabelStyle(s_axis_label_font.file(), s_axis_label_font.size(), Chart::TextColor, TOP_DOWN_LABEL_ANGLE);

		vector<t_string> name_strings;
		name_strings.reserve(name_count);
		for (auto info(zoomed); info; ++info)
			name_strings.push_back(LPCSTR(TCHARtoUTF8(info->component_name_cd().c_str())));
		vector<const t_char*> names;
		names.reserve(name_count);
		for (auto& name_string : name_strings)
			names.push_back(name_string.c_str());
		auto textbox = name_axis->setLabels(StringArray(names.data(), int(names.size())));
		if (m_show_component_icons)
			textbox->setPos(textbox->getLeftX(), textbox->getTopY() + ICON_HEIGHT);

		name_axis->setTickOffset(0.5);
		name_axis->setRounding(false, false);
		name_axis->setMargin(0, 0);

		BarLayer *layer = chart.addBarLayer(Chart::Side);
		m_component_bar_set.add_to_layer(layer);
		layer->setBarGap(0.4, 0.0);
		layer->setBorderColor(Chart::Transparent);
	}

	// Plot right axis grid lines
	if (show_axes)
	{
		chart.layout();
		int right_mark_color = hgrid_color(use_right_axis ? m_plot_color_right->m_mark_color : m_plot_color_left->m_mark_color);
		DoubleArray ticks(right_axis->getTicks());
		for (int i = 0; i < ticks.len; ++i)
		{
			if (*right_axis->getLabel(ticks[i]))
			{
				Mark* mark = right_axis->addMark(ticks[i], right_mark_color);
				mark->setDrawOnTop(false);
			}
		}
	}

	chart_ptr->makeChart();

	if (m_show_component_icons && zoomed)
	{
		double xinc	= plot_width/double(name_count);
		double x	= m_plot_bounds.left + 0.5 * xinc;
		int y		= m_plot_bounds.bottom + ICON_MARGIN;
		for (const auto& info : range_wrapper(zoomed))
		{
			chart_ptr->getDrawArea()->merge(
				&component_cd_bitmap(info.component().type()),
				round<int>(x), y,
				Chart::TopCenter, 0
			);
			x += xinc;
		}
	}

	return chart_ptr;
}

