/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Declaration of RedTinLogicAnalyzer
 */

#ifndef AntikernelLogicAnalyzer_h
#define AntikernelLogicAnalyzer_h

class AntikernelLogicAnalyzer
	: public virtual Oscilloscope
	, public SCPIDevice
{
public:
	AntikernelLogicAnalyzer(SCPITransport* transport);
	virtual ~AntikernelLogicAnalyzer();

	virtual std::string GetTransportConnectionString();
	virtual std::string GetTransportName();
	virtual std::string GetDriverName();

	virtual std::string GetName();
	virtual std::string GetVendor();
	virtual std::string GetSerial();

	//Channel configuration
	virtual bool IsChannelEnabled(size_t i);
	virtual void EnableChannel(size_t i);
	virtual void DisableChannel(size_t i);
	virtual OscilloscopeChannel::CouplingType GetChannelCoupling(size_t i);
	virtual void SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type);
	virtual double GetChannelAttenuation(size_t i);
	virtual void SetChannelAttenuation(size_t i, double atten);
	virtual int GetChannelBandwidthLimit(size_t i);
	virtual void SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz);
	virtual double GetChannelVoltageRange(size_t i);
	virtual void SetChannelVoltageRange(size_t i, double range);
	virtual OscilloscopeChannel* GetExternalTrigger();
	virtual double GetChannelOffset(size_t i);
	virtual void SetChannelOffset(size_t i, double offset);

	//Triggering
	virtual Oscilloscope::TriggerMode PollTrigger();
	virtual bool AcquireData(bool toQueue = false);
	virtual void Start();
	virtual void StartSingleTrigger();
	virtual void Stop();

	virtual bool IsTriggerArmed();
	virtual size_t GetTriggerChannelIndex();
	virtual void SetTriggerChannelIndex(size_t i);
	virtual float GetTriggerVoltage();
	virtual void SetTriggerVoltage(float v);
	virtual Oscilloscope::TriggerType GetTriggerType();
	virtual void SetTriggerType(Oscilloscope::TriggerType type);

	virtual std::vector<uint64_t> GetSampleRatesNonInterleaved();
	virtual std::vector<uint64_t> GetSampleRatesInterleaved();
	virtual std::set<InterleaveConflict> GetInterleaveConflicts();
	virtual std::vector<uint64_t> GetSampleDepthsNonInterleaved();
	virtual std::vector<uint64_t> GetSampleDepthsInterleaved();

	virtual void ResetTriggerConditions();
	virtual void SetTriggerForChannel(OscilloscopeChannel* channel, std::vector<TriggerType> triggerbits);

	virtual unsigned int GetInstrumentTypes();

protected:
	void LoadChannels();

	void SendCommand(uint8_t opcode);
	void SendCommand(uint8_t opcode, uint8_t chan);
	void SendCommand(uint8_t opcode, uint8_t chan, uint8_t arg);
	uint8_t Read1ByteReply();

	void ArmTrigger();

	bool m_triggerArmed;
	bool m_triggerOneShot;

	std::recursive_mutex m_mutex;

	std::vector<size_t> m_lowIndexes;
	std::vector<size_t> m_highIndexes;

	uint32_t m_samplePeriod;
	uint32_t m_memoryDepth;
	uint32_t m_memoryWidth;
	uint32_t m_maxWidth;
};

#endif

