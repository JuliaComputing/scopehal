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

#include "scopeprotocols.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MovingAverageFilter::MovingAverageFilter(const string& color)
	: Filter(OscilloscopeChannel::CHANNEL_TYPE_ANALOG, color, CAT_MATH)
{
	//Set up channels
	CreateInput("din");

	m_depthname = "Depth";
	m_parameters[m_depthname] = FilterParameter(FilterParameter::TYPE_INT, Unit(Unit::UNIT_SAMPLEDEPTH));
	m_parameters[m_depthname].SetFloatVal(0);

	m_range = 0;
	m_offset = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool MovingAverageFilter::ValidateChannel(size_t i, StreamDescriptor stream)
{
	if(stream.m_channel == NULL)
		return false;

	if( (i == 0) && (stream.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG) )
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

float MovingAverageFilter::GetVoltageRange(size_t /*stream*/)
{
	return m_range;
}

float MovingAverageFilter::GetOffset(size_t /*stream*/)
{
	return m_offset;
}

void MovingAverageFilter::SetVoltageRange(float range, size_t /*stream*/)
{
	m_range = range;
}

void MovingAverageFilter::SetOffset(float offset, size_t /*stream*/)
{
	m_offset = offset;
}

string MovingAverageFilter::GetProtocolName()
{
	return "Moving average";
}

bool MovingAverageFilter::NeedsConfig()
{
	//we need the depth to be specified, duh
	return true;
}

void MovingAverageFilter::SetDefaultName()
{
	char hwname[256];
	snprintf(hwname, sizeof(hwname), "MovingAvg(%s, %s)",
		GetInputDisplayName(0).c_str(),
		m_parameters[m_depthname].ToString().c_str());
	m_hwname = hwname;
	m_displayname = m_hwname;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void MovingAverageFilter::Refresh()
{
	if(!VerifyAllInputsOKAndAnalog())
	{
		SetData(NULL, 0);
		return;
	}

	//Get the input data
	auto din = GetAnalogInputWaveform(0);
	size_t len = din->m_samples.size();
	size_t depth = m_parameters[m_depthname].GetIntVal();
	if(len < depth)
	{
		SetData(NULL, 0);
		return;
	}

	m_xAxisUnit = m_inputs[0].m_channel->GetXAxisUnits();
	SetYAxisUnits(m_inputs[0].GetYAxisUnits(), 0);

	//Do the average
	auto cap = new AnalogWaveform;
	size_t nsamples = len - depth;
	size_t off = depth/2;
	cap->Resize(nsamples);
	//#pragma omp parallel for
	for(size_t i=0; i<nsamples; i++)
	{
		float v = 0;
		for(size_t j=0; j<depth; j++)
			v += din->m_samples[i+j];
		v /= depth;

		cap->m_offsets[i] = din->m_offsets[i+off];
		cap->m_durations[i] = din->m_durations[i+off];
		cap->m_samples[i] = v;
	}
	SetData(cap, 0);

	//Calculate bounds
	if(m_range == 0)
	{
		float vmin = FLT_MAX;
		float vmax = -FLT_MAX;
		for(size_t i=0; i<nsamples; i++)
		{
			float v = cap->m_samples[i];
			vmin = min(vmin, v);
			vmax = max(vmax, v);
		}
		m_range = (vmax - vmin) * 1.05;
		m_offset = -( (vmax - vmin)/2 + m_min );
	}

	//Copy our time scales from the input
	cap->m_timescale = din->m_timescale;
	cap->m_startTimestamp = din->m_startTimestamp;
	cap->m_startFemtoseconds = din->m_startFemtoseconds;
}
