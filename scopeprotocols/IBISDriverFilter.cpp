/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "../scopehal/scopehal.h"
#include "IBISDriverFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

IBISDriverFilter::IBISDriverFilter(const string& color)
	: Filter(OscilloscopeChannel::CHANNEL_TYPE_ANALOG, color, CAT_GENERATION)
	, m_model(NULL)
	, m_sampleRate("Sample Rate")
	, m_fname("File Path")
	, m_modelName("Model Name")
	, m_cornerName("Corner")
	, m_termName("Termination")
{
	CreateInput("data");
	CreateInput("clk");

	m_parameters[m_sampleRate] = FilterParameter(FilterParameter::TYPE_INT, Unit(Unit::UNIT_SAMPLERATE));
	m_parameters[m_sampleRate].SetIntVal(100 * 1000L * 1000L * 1000L);	//100 Gsps

	m_parameters[m_fname] = FilterParameter(FilterParameter::TYPE_FILENAME, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_fname].m_fileFilterMask = "*.ibs";
	m_parameters[m_fname].m_fileFilterName = "IBIS model files (*.ibs)";
	m_parameters[m_fname].signal_changed().connect(sigc::mem_fun(this, &IBISDriverFilter::OnFnameChanged));

	m_parameters[m_modelName] = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_modelName].signal_changed().connect(sigc::mem_fun(this, &IBISDriverFilter::OnModelChanged));

	m_parameters[m_cornerName] = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_cornerName].AddEnumValue("Minimum", CORNER_MIN);
	m_parameters[m_cornerName].AddEnumValue("Typical", CORNER_TYP);
	m_parameters[m_cornerName].AddEnumValue("Maximum", CORNER_MAX);
	m_parameters[m_cornerName].SetIntVal(CORNER_TYP);

	m_parameters[m_termName] = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));

	ClearSweeps();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool IBISDriverFilter::ValidateChannel(size_t i, StreamDescriptor stream)
{
	if(stream.m_channel == NULL)
		return false;

	if( (i < 2) && (stream.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) )
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string IBISDriverFilter::GetProtocolName()
{
	return "IBIS Driver";
}

void IBISDriverFilter::SetDefaultName()
{
	char hwname[256];
	snprintf(hwname, sizeof(hwname), "IBIS(%s)", GetInputDisplayName(0).c_str());
	m_hwname = hwname;
	m_displayname = m_hwname;
}

float IBISDriverFilter::GetVoltageRange(size_t /*stream*/)
{
	return m_range;
}

float IBISDriverFilter::GetOffset(size_t /*stream*/)
{
	return m_offset;
}

bool IBISDriverFilter::NeedsConfig()
{
	return true;
}

void IBISDriverFilter::ClearSweeps()
{
	m_vmax = FLT_MIN;
	m_vmin = FLT_MAX;
	m_range = 1;
	m_offset = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void IBISDriverFilter::LoadParameters(const YAML::Node& node, IDTable& table)
{
	Filter::LoadParameters(node, table);
	m_parameters[m_modelName].Reinterpret();
}

void IBISDriverFilter::OnFnameChanged()
{
	//Load the IBIS model
	m_parser.Clear();
	m_parser.Load(m_parameters[m_fname].ToString());
	m_model = NULL;

	//Make a list of candidate output models
	vector<string> names;
	for(auto it : m_parser.m_models)
	{
		//Skip any models other than push-pull output and I/O
		auto model = it.second;
		if( (model->m_type != IBISModel::TYPE_OUTPUT) && (model->m_type != IBISModel::TYPE_IO) )
			continue;

		names.push_back(it.first);
	}

	//Recreate the list of options
	std::sort(names.begin(), names.end());
	m_parameters[m_modelName].ClearEnumValues();
	for(size_t i=0; i<names.size(); i++)
		m_parameters[m_modelName].AddEnumValue(names[i], i);

	//TODO: update enum models etc

	//Min/max are likely invalid now
	ClearSweeps();
}

void IBISDriverFilter::OnModelChanged()
{
	m_model = m_parser.m_models[m_parameters[m_modelName].ToString()];

	//Recreate list of terminations
	Unit ohms(Unit::UNIT_OHMS);
	Unit volts(Unit::UNIT_VOLTS);
	m_parameters[m_termName].ClearEnumValues();
	for(size_t i=0; i<m_model->m_rising.size(); i++)
	{
		auto& w = m_model->m_rising[i];
		auto ename = ohms.PrettyPrint(w.m_fixtureResistance) + " to " + volts.PrettyPrint(w.m_fixtureVoltage);
		m_parameters[m_termName].AddEnumValue(ename, i);
	}

	//Min/max are likely invalid now
	ClearSweeps();
}

void IBISDriverFilter::Refresh()
{
	//If we don't have a model, nothing to do
	if(!VerifyAllInputsOK() || !m_model)
	{
		SetData(NULL, 0);
		return;
	}

	//Get the input and sample it
	auto din = GetDigitalInputWaveform(0);
	auto clkin = GetDigitalInputWaveform(1);
	DigitalWaveform samples;
	SampleOnAnyEdges(din, clkin, samples);

	size_t rate = m_parameters[m_sampleRate].GetIntVal();
	if(rate == 0)
	{
		SetData(NULL, 0);
		return;
	}
	size_t samplePeriod = FS_PER_SECOND / rate;

	//Configure output waveform
	auto cap = SetupEmptyOutputWaveform(din, 0);
	cap->m_timescale = samplePeriod;
	cap->m_densePacked = true;

	//Round length to integer number of complete cycles
	size_t len = samples.m_samples.size();

	//Adjust for start time
	int64_t capstart = samples.m_offsets[0];
	cap->m_triggerPhase = capstart;

	//Figure out how long the capture is going to be
	size_t caplen = (samples.m_offsets[len-1] + samples.m_durations[len-1] - capstart) / samplePeriod;
	cap->Resize(caplen);

	//Find the rising edge waveform - easy
	auto risingTerm = m_parameters[m_termName].GetIntVal();
	VTCurves& rising = m_model->m_rising[risingTerm];

	//Find the falling edge waveform. We have to search all of them because they might not be in the same order!!
	size_t fallingTerm=0;
	for(; fallingTerm < m_model->m_falling.size(); fallingTerm ++)
	{
		if(
			( (m_model->m_falling[fallingTerm].m_fixtureResistance - rising.m_fixtureResistance) < 0.01) &&
			( (m_model->m_falling[fallingTerm].m_fixtureVoltage - rising.m_fixtureVoltage) < 0.01) )
		{
			break;
		}
	}
	VTCurves& falling = m_model->m_falling[fallingTerm];
	auto corner = static_cast<IBISCorner>(m_parameters[m_cornerName].GetIntVal());

	//Figure out the propagation delay of the buffers for rising and falling edges
	int64_t rising_delay = rising.GetPropagationDelay(corner);
	int64_t falling_delay = falling.GetPropagationDelay(corner);

	//Make a list of rising/falling edges in the incoming data stream
	bool last = samples.m_samples[0];
	vector<bool> edgeDirections;
	vector<int64_t> edgeTimestamps;
	for(size_t i=1; i<len; i++)
	{
		bool b = samples.m_samples[i];
		if(b != last)
		{
			last = b;
			edgeDirections.push_back(b);
			edgeTimestamps.push_back(samples.m_offsets[i]);
		}
	}

	//Sanity check that we actually have some data
	if(edgeTimestamps.empty())
	{
		SetData(NULL, 0);
		return;
	}

	//Generate output samples at uniform intervals
	size_t iedge = 0;
	for(size_t i=0; i<caplen; i++)
	{
		//Default fill of timestamps etc
		cap->m_offsets[i] = i;
		cap->m_durations[i] = 1;

		//Timestamp of the current output sample
		int64_t tnow = cap->m_timescale*i + cap->m_triggerPhase;

		//Find timestamp of the next edge (including buffer propagation delay)
		if( (iedge + 1) < edgeTimestamps.size())
		{
			//Nominal timestamp of the edge
			int64_t tnextedge = edgeTimestamps[iedge+1];

			//Shift by the buffer delay
			int64_t tdelayed = tnextedge;
			if(edgeDirections[iedge+1])
				tdelayed += rising_delay;
			else
				tdelayed += falling_delay;

			//Move to the next edge if we're past the initial propagation delay of the upcoming edge
			if(tnow >= tdelayed)
				iedge ++;
		}

		//Time since the edge started
		int64_t relative_timestamp = tnow - edgeTimestamps[iedge];
		float rel_sec = relative_timestamp * SECONDS_PER_FS;
		float v;
		if(edgeDirections[iedge])
			v = rising.InterpolateVoltage(corner, rel_sec);
		else
			v = falling.InterpolateVoltage(corner, rel_sec);
		cap->m_samples[i] = v;

		m_vmax = max(m_vmax, v);
		m_vmin = min(m_vmin, v);
	}

	m_range = (m_vmax - m_vmin) * 1.05;
	m_offset = -( (m_vmax - m_vmin)/2 + m_vmin );
}
