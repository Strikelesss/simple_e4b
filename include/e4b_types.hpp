#pragma once
#include <array>
#include <cassert>
#include <string_view>
#include <fstream>

namespace byteswap_helpers
{
	[[nodiscard]] constexpr unsigned short byteswap_uint16(const unsigned short val) noexcept
	{
		return static_cast<unsigned short>((val << 8) | (val >> 8));
	}

	[[nodiscard]] constexpr unsigned long byteswap_uint32(const unsigned long val) noexcept
	{
		return (val << 24) | ((val << 8) & 0x00FF'0000) | ((val >> 8) & 0x0000'FF00) | (val >> 24);
	}
}

namespace simple_e4b
{
	constexpr std::array<std::string_view, 12> MIDI_NOTATION{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	constexpr int8_t MIDI_OCTAVE_MIN = -2i8;
	constexpr int8_t MIDI_OCTAVE_MAX = 8i8;
	constexpr int8_t MIN_TRANSPOSE_BYTE = -36i8;
	constexpr int8_t MAX_TRANSPOSE_BYTE = 36i8;
	constexpr int8_t MIN_COARSE_TUNE_BYTE = -72i8;
	constexpr int8_t MAX_COARSE_TUNE_BYTE = 24i8;
	constexpr int8_t MIN_VOLUME_BYTE = -96i8;
	constexpr int8_t MAX_VOLUME_BYTE = 10i8;
	constexpr int8_t MIN_PAN_BYTE = -64i8;
	constexpr int8_t MAX_PAN_BYTE = 63i8;
	constexpr uint8_t MIN_LFO_LAG_BYTE = 0i8;
	constexpr uint8_t MAX_LFO_LAG_BYTE = 10i8;
	constexpr uint8_t MIN_ZONE_DATA_BYTE = 0i8;
	constexpr uint8_t MAX_ZONE_DATA_BYTE = 127i8;
	constexpr uint16_t MIN_FILTER_FREQUENCY = 57i16;
	constexpr uint16_t MAX_FILTER_FREQUENCY = 20000i16;
	
	constexpr uint32_t EOS_E4_TOC_SIZE = 32u;
	constexpr size_t EOS_E4_MAX_PRESETS = 1000;
	constexpr size_t EOS_E4_MAX_VOICES = std::numeric_limits<uint16_t>::max();
	constexpr size_t EOS_E4_MAX_SAMPLES = 1000;
	constexpr size_t EOS_E4_MAX_SEQUENCES = 1000;
	constexpr size_t EOS_E4_MAX_ZONES = 256;
	constexpr size_t FORM_CHUNK_MAX_NAME_LEN = 4;
	constexpr size_t EOS_E4_MAX_NAME_LEN = 16;
	constexpr size_t EOS_NUM_EXTRA_SAMPLE_PARAMETERS = 8;
	constexpr uint8_t EOS_E4_INITIAL_MIDI_CONTROLLER_OFF = std::numeric_limits<uint8_t>::max();

	inline void ApplyEOSNamingStandards(std::string& str)
	{
		assert(!str.empty());
		if(!str.empty() && str.length() != EOS_E4_MAX_NAME_LEN)
		{
			str.resize(EOS_E4_MAX_NAME_LEN);
			std::replace(str.begin(), str.end(), '\0', ' ');
		}
	}
	
	/*
	 * Chunks:
	 */

	struct FORMChunk final
	{
		FORMChunk() = default;
		
		explicit FORMChunk(std::string&& chunkName, const uint32_t chunkSize = 0u)
			: m_chunkName(std::move(chunkName)), m_readChunkSize(chunkSize) {}
		
		void Write(std::ofstream& stream) const
		{
			if(m_chunkName.length() != FORM_CHUNK_MAX_NAME_LEN)
			{
				assert(m_chunkName.length() == FORM_CHUNK_MAX_NAME_LEN);
				return;
			}
			
			stream.write(m_chunkName.data(), FORM_CHUNK_MAX_NAME_LEN);

			// We override the chunk size here specifically for the TOC subchunks.
			const uint32_t chunkSize(byteswap_helpers::byteswap_uint32(m_readChunkSize > 0u ? m_readChunkSize : GetFullSize(true) - 8u));
			stream.write(reinterpret_cast<const char*>(&chunkSize), sizeof(uint32_t));

			if(!m_writtenData.empty())
			{
				stream.write(m_writtenData.data(), static_cast<std::streamsize>(m_writtenData.size()));	
			}
			
			for(const auto& subchunk : m_subChunks)
			{
				subchunk.Write(stream);
			}
		}
		
		void Read(std::ifstream& stream)
		{
			m_chunkName.clear();
			m_chunkName.resize(FORM_CHUNK_MAX_NAME_LEN);
			stream.read(m_chunkName.data(), FORM_CHUNK_MAX_NAME_LEN);
			
			stream.read(reinterpret_cast<char*>(&m_readChunkSize), sizeof(uint32_t));
			m_readChunkSize = byteswap_helpers::byteswap_uint32(m_readChunkSize);
		}

		[[nodiscard]] std::string_view GetName() const { return m_chunkName; }

		[[nodiscard]] uint32_t GetReadSize() const
		{
			return m_readChunkSize;
		}
		
		[[nodiscard]] uint32_t GetFullSize(const bool includeHeader) const
		{
			uint32_t chunkSize(static_cast<uint32_t>(m_writtenData.size()));

			if(includeHeader)
			{
				chunkSize += static_cast<uint32_t>(GetName().length()) + sizeof(uint32_t);	
			}

			for (const auto& subchunk : m_subChunks)
			{
				chunkSize += subchunk.GetFullSize(includeHeader);
			}

			return chunkSize;	
		}
		
		template<typename T>
		void writeType(const T* data, const size_t size = sizeof(T))
		{
			static_assert(std::is_fundamental_v<T>);

			assert(size > 0);
			if(size > 0)
			{
				if (m_writtenData.size() + size > m_writtenData.size())
				{
					m_writtenData.resize(m_writtenData.size() + size);
				}

				if(data != nullptr)
				{
					char* iterator(std::next(m_writtenData.data(), static_cast<ptrdiff_t>(m_writeLocation)));
					std::memcpy(iterator, data, size);
				}

				m_writeLocation += size;	
			}
		}

		std::vector<FORMChunk> m_subChunks{};

	private:
		std::string m_chunkName;
		uint32_t m_readChunkSize = 0u;

		std::vector<char> m_writtenData{};
		size_t m_writeLocation = 0;
	};

	/*
	 * Voices:
	 */

	namespace unit_helpers
	{
		constexpr double MAX_FREQUENCY_20000 = 9.90348755253612804;
		constexpr double MIN_FREQUENCY_57 = 4.04305126783455015;
		constexpr double MAX_FREQUENCY_BYTE = 255.0;
		constexpr double MAX_FINE_TUNE_BYTE = 64.0;
		constexpr float MIN_CHORUS_WIDTH = 0.78125f;
		constexpr double MIN_FINE_TUNE = 1.5625;

		inline double round_d_places(const double value, const uint32_t places)
		{
			double convertedPlaces(1.0);
			for(uint32_t i(0u); i < places; ++i) { convertedPlaces *= 10.0; }
			return std::ceil(value * convertedPlaces) / convertedPlaces;
		}

		inline float round_f_places(const float value, const uint32_t places)
		{
			float convertedPlaces(1.f);
			for(uint32_t i(0u); i < places; ++i) { convertedPlaces *= 10.f; }
			return std::ceilf(value * convertedPlaces) / convertedPlaces;
		}
		
		inline uint16_t ConvertByteToFilterFrequency(const std::uint8_t b)
		{
			const double t(static_cast<double>(b) / MAX_FREQUENCY_BYTE);
			return static_cast<uint16_t>(std::round(std::exp(t * (MAX_FREQUENCY_20000 - MIN_FREQUENCY_57) + MIN_FREQUENCY_57)));
		}

		inline uint8_t ConvertFilterFrequencyToByte(const uint16_t filterFreq)
		{
			return static_cast<uint8_t>(std::round((std::log(filterFreq) - MIN_FREQUENCY_57) / (MAX_FREQUENCY_20000 - MIN_FREQUENCY_57) * MAX_FREQUENCY_BYTE));
		}

		// [-100, 100] to [-64, 64]
		inline int8_t ConvertFineTuneToByte(const double fineTune)
		{
			return static_cast<int8_t>(std::round((fineTune - 100.0) / MIN_FINE_TUNE + MAX_FINE_TUNE_BYTE));
		}

		// [-64, 64] to [-100, 100]
		inline double ConvertByteToFineTune(const int8_t b)
		{
			return round_d_places((static_cast<double>(b) - MAX_FINE_TUNE_BYTE) * MIN_FINE_TUNE + 100.0, 2u);
		}

		// [0, 127] to [0.08, 18.01]
		inline double GetLFORateFromByte(const uint8_t b)
		{
			constexpr double a1(1.64054); constexpr double b1(1.01973); constexpr double c1(-1.57702);
			return a1 * std::pow(b1, b) + c1;
		}

		// [0.08, 18.01] to [0, 127]
		inline uint8_t GetByteFromLFORate(const double rate)
		{
			constexpr double a1(1.64054); constexpr double b1(1.01973); constexpr double c1(-1.57702);
			return static_cast<uint8_t>(std::round(std::log((rate - c1) / a1) / std::log(b1)));
		}

		// [-128, 0] to [0%, 100%]
		inline float GetChorusWidthPercent(const uint8_t value)
		{
			return std::clamp(round_f_places(std::abs((static_cast<float>(value) - 128.f) * MIN_CHORUS_WIDTH), 2u), 0.f, 100.f);
		}

		// [0%, 100%] to [-128, 0]
		inline uint8_t ConvertChorusWidthToByte(const float value)
		{
			return static_cast<int8_t>(value / MIN_CHORUS_WIDTH + 128.f);
		}

		// [0%, 100%] to [0, 127]
		inline int8_t ConvertPercentToByteF(const float value)
		{
			return static_cast<int8_t>(std::roundf(value * 127.f / 100.f));
		}

		template<typename T>
		[[nodiscard]] constexpr float ConvertByteToPercentF(const T b)
		{
			// Make sure we're only converting bytes
			static_assert(sizeof(T) == 1);

			return static_cast<float>(b) / 127.f * 100.f;
		}

		inline double GetLFODelayFromByte(const uint8_t b)
		{
			constexpr double a1(0.149998); constexpr double b1(1.04); constexpr double c1(-0.150012);
			return a1 * std::pow(b1, b) + c1;
		}

		inline uint8_t GetByteFromLFODelay(const double delay)
		{
			constexpr double a1(0.149998); constexpr double b1(1.04); constexpr double c1(-0.150012);
			return static_cast<uint8_t>(std::round(std::log((delay - c1) / a1) / std::log(b1)));
		}
	}

	struct MidiNote final
	{
		explicit MidiNote(const uint8_t byte) : m_notation(MIDI_NOTATION[byte % 12]), m_octave(static_cast<int8_t>(static_cast<int>(byte) / 12 - 2)) {}
		
		explicit MidiNote(const std::string_view notation, const int8_t octave)
			: m_octave(std::clamp(octave, MIDI_OCTAVE_MIN, MIDI_OCTAVE_MAX))
		{
			SetNotation(notation);
		}

		[[nodiscard]] uint8_t ToByte() const
		{
			const int notationPos(static_cast<int>(std::distance(MIDI_NOTATION.begin(), std::find(MIDI_NOTATION.begin(), MIDI_NOTATION.end(), m_notation))));
			return static_cast<uint8_t>(std::clamp(12 + notationPos + (m_octave + 1) * 12, 0, 127));	
		}

		void SetNotation(const std::string_view notation)
		{
			const bool isValidNotation(std::find(MIDI_NOTATION.begin(), MIDI_NOTATION.end(), notation) != MIDI_NOTATION.end());
			assert(isValidNotation);
			if(isValidNotation) { m_notation = notation; }
		}

		void SetOctave(const int8_t octave)
		{
			m_octave = std::clamp(octave, MIDI_OCTAVE_MIN, MIDI_OCTAVE_MAX);
		}

	private:
		std::string_view m_notation;
		int8_t m_octave = 0i8;
	};

	struct E4SampleZoneNoteData final
	{
		E4SampleZoneNoteData() = default;

		explicit E4SampleZoneNoteData(const uint8_t low, const uint8_t high) : m_low(low), m_high(high) {}

		explicit E4SampleZoneNoteData(const uint8_t low, const uint8_t lowFade, const uint8_t highFade, const uint8_t high)
			: m_low(low), m_lowFade(lowFade), m_highFade(highFade), m_high(high) {}

		void SetLow(const uint8_t val) { m_low = std::clamp(val, MIN_ZONE_DATA_BYTE, MAX_ZONE_DATA_BYTE); }
		void SetHigh(const uint8_t val) { m_high = std::clamp(val, MIN_ZONE_DATA_BYTE, MAX_ZONE_DATA_BYTE); }
		void SetLowFade(const uint8_t val) { m_lowFade = std::clamp(val, MIN_ZONE_DATA_BYTE, MAX_ZONE_DATA_BYTE); }
		void SetHighFade(const uint8_t val) { m_highFade = std::clamp(val, MIN_ZONE_DATA_BYTE, MAX_ZONE_DATA_BYTE); }

		[[nodiscard]] uint8_t GetLow() const { return m_low; }
		[[nodiscard]] uint8_t GetHigh() const { return m_high; }
		[[nodiscard]] uint8_t GetLowFade() const { return m_lowFade; }
		[[nodiscard]] uint8_t GetHighFade() const { return m_highFade; }
		
	private:
		uint8_t m_low = 0ui8; // [0, 127]
		uint8_t m_lowFade = 0ui8; // [0, 127]
		uint8_t m_highFade = 0ui8; // [0, 127] 
		uint8_t m_high = 127ui8; // [0, 127]
	};
	
	struct E4SampleZone final
	{
		E4SampleZone() = default;
		
		explicit E4SampleZone(const uint16_t sampleIndex, const MidiNote& originalKey)
			: m_sampleIndex(sampleIndex), m_originalKey(originalKey) {}

		void Write(FORMChunk& presetChunk) const
		{
			presetChunk.writeType(reinterpret_cast<const char*>(&m_keyData), sizeof(E4SampleZoneNoteData));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_velData), sizeof(E4SampleZoneNoteData));
			
			const uint16_t sampleIndex(byteswap_helpers::byteswap_uint16(m_sampleIndex));
			presetChunk.writeType(reinterpret_cast<const char*>(&sampleIndex), sizeof(uint16_t));

			const char* null(nullptr);
			presetChunk.writeType(null, 1);

			const int8_t fineTune(unit_helpers::ConvertFineTuneToByte(m_fineTune));
			presetChunk.writeType(reinterpret_cast<const char*>(&fineTune), sizeof(int8_t));
			
			const uint8_t originalKey(m_originalKey.ToByte());
			presetChunk.writeType(reinterpret_cast<const char*>(&originalKey), sizeof(uint8_t));
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_volume), sizeof(int8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_pan), sizeof(int8_t));

			presetChunk.writeType(null, 7);
		}
		
		void Read(std::ifstream& stream)
		{
			stream.read(reinterpret_cast<char*>(&m_keyData), sizeof(E4SampleZoneNoteData));
			stream.read(reinterpret_cast<char*>(&m_velData), sizeof(E4SampleZoneNoteData));
			
			stream.read(reinterpret_cast<char*>(&m_sampleIndex), sizeof(uint16_t));
			m_sampleIndex = byteswap_helpers::byteswap_uint16(m_sampleIndex);

			stream.ignore(1);

			int8_t fineTune;
			stream.read(reinterpret_cast<char*>(&fineTune), sizeof(int8_t));

			m_fineTune = unit_helpers::ConvertByteToFineTune(fineTune);

			uint8_t originalKey;
			stream.read(reinterpret_cast<char*>(&originalKey), sizeof(uint8_t));

			m_originalKey = MidiNote(originalKey);
			
			stream.read(reinterpret_cast<char*>(&m_volume), sizeof(int8_t));
			stream.read(reinterpret_cast<char*>(&m_pan), sizeof(int8_t));

			stream.ignore(7);
		}

		void SetSampleIndex(const uint16_t index) { m_sampleIndex = index; }
		void SetFineTune(const double fineTune) { m_fineTune = std::clamp(fineTune, -100.0, 100.0); }
		void SetVolume(const int8_t dB) { m_volume = std::clamp(dB, MIN_VOLUME_BYTE, MAX_VOLUME_BYTE); }
		void SetPan(const int8_t pan) { m_pan = std::clamp(pan, MIN_PAN_BYTE, MAX_PAN_BYTE); }

		/**
		 * \brief 
		 * \return Volume in dB
		 */
		[[nodiscard]] int8_t GetVolume() const { return m_volume; }
		
		[[nodiscard]] E4SampleZoneNoteData& GetKeyData() { return m_keyData; }
		[[nodiscard]] E4SampleZoneNoteData& GetVelData() { return m_velData; }
		[[nodiscard]] const E4SampleZoneNoteData& GetKeyData() const { return m_keyData; }
		[[nodiscard]] const E4SampleZoneNoteData& GetVelData() const { return m_velData; }
		[[nodiscard]] uint16_t GetSampleIndex() const { return m_sampleIndex; }
		[[nodiscard]] double GetFineTune() const { return m_fineTune; }
		[[nodiscard]] const MidiNote& GetOriginalKey() const { return m_originalKey; }
		[[nodiscard]] int8_t GetPan() const { return m_pan; }

	private:
		E4SampleZoneNoteData m_keyData;
		E4SampleZoneNoteData m_velData;

		uint16_t m_sampleIndex = 0ui16; // requires byteswap
		double m_fineTune = 0.0;

		MidiNote m_originalKey = MidiNote(MIDI_NOTATION[0], 3); // [0, 127]
		int8_t m_volume = 0i8; // [-96, 10]
		int8_t m_pan = 0i8; // [-64, 63]
	};

	struct E4Envelope final
	{
		E4Envelope() = default;
		
		// Either of the attacks can be 'Attack'
		// E.G. if Attack1 level is 0, Attack2 serves as 'Attack'
		// or if Attack1 level is 100, Attack1 serves as 'Attack'
		uint8_t m_attack1Sec = 0ui8;
		int8_t m_attack1Level = 0i8;
		uint8_t m_attack2Sec = 0ui8;
		int8_t m_attack2Level = 127i8;

		uint8_t m_decay1Sec = 0ui8; // A.K.A. 'Hold'
		int8_t m_decay1Level = 127i8;
		uint8_t m_decay2Sec = 0ui8; // A.K.A. 'Decay'
		int8_t m_decay2Level = 127i8; // A.K.A. 'Sustain'

		uint8_t m_release1Sec = 0ui8; // A.K.A. 'Release'
		int8_t m_release1Level = 0i8;
		uint8_t m_release2Sec = 0ui8;
		int8_t m_release2Level = 0i8;
	};

	enum struct E4LFOShape final : uint8_t
	{
		TRIANGLE = 0ui8, SINE = 1ui8, SAWTOOTH = 2ui8, SQUARE = 3ui8,
		PULSE_33 = 4ui8, PULSE_25 = 5ui8, PULSE_16 = 6ui8, PULSE_12 = 7ui8,
		OCTAVES = 8ui8, FIFTH_PLUS_OCTAVE = 9ui8, SUS4_TRIP = 10ui8,
		NEENER = 11ui8, SINE_1_2 = 12ui8, SINE_1_3_5 = 13ui8, SINE_NOISE = 14ui8,
		HEMI_QUAVER = 15ui8, RANDOM = 255ui8
	};

	struct E4LFO final
	{
		E4LFO() = default;

		explicit E4LFO(const double rate, const E4LFOShape shape, const double delay, const float variation, const bool keySync)
			: m_rate(rate), m_shape(shape), m_delay(delay), m_variationPercent(variation), m_keySync(keySync) {}

		void Write(FORMChunk& presetChunk) const
		{
			const uint8_t rate(unit_helpers::GetByteFromLFORate(std::clamp(m_rate, 0.08, 18.01)));
			presetChunk.writeType(reinterpret_cast<const char*>(&rate), sizeof(uint8_t));
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_shape), sizeof(E4LFOShape));

			const uint8_t delay(unit_helpers::GetByteFromLFODelay(std::clamp(m_delay, 0.0, 21.694)));
			presetChunk.writeType(reinterpret_cast<const char*>(&delay), sizeof(uint8_t));

			const uint8_t variation(unit_helpers::ConvertPercentToByteF(std::clamp(m_variationPercent, 0.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&variation), sizeof(uint8_t));

			const bool keySync(!m_keySync);
			presetChunk.writeType(reinterpret_cast<const char*>(&keySync), sizeof(bool));
			
			const char* null(nullptr);
			presetChunk.writeType(null, 2);
		}
		
		void Read(std::ifstream& stream)
		{
			uint8_t rate;
			stream.read(reinterpret_cast<char*>(&rate), sizeof(uint8_t));

			m_rate = unit_helpers::GetLFORateFromByte(rate);
			
			stream.read(reinterpret_cast<char*>(&m_shape), sizeof(E4LFOShape));

			uint8_t delay;
			stream.read(reinterpret_cast<char*>(&delay), sizeof(uint8_t));

			m_delay = unit_helpers::GetLFODelayFromByte(delay);

			uint8_t variation;
			stream.read(reinterpret_cast<char*>(&variation), sizeof(uint8_t));

			m_variationPercent = unit_helpers::ConvertByteToPercentF(variation);
			
			stream.read(reinterpret_cast<char*>(&m_keySync), sizeof(bool));

			// Flipped:
			m_keySync = !m_keySync;

			stream.ignore(2);
		}

		void SetRate(const double hertz)
		{
			m_rate = std::clamp(hertz, 0.08, 18.01);
		}

		void SetDelay(const double delaySec)
		{
			m_delay = std::clamp(delaySec, 0.0, 21.694);
		}

		void SetVariationPercent(const float variationPercent)
		{
			m_variationPercent = std::clamp(variationPercent, 0.f, 100.f);
		}

		void SetShape(const E4LFOShape shape) { m_shape = shape; }
		void SetKeySync(const bool arg) { m_keySync = arg; }

		[[nodiscard]] E4LFOShape GetShape() const { return m_shape; }
		[[nodiscard]] bool IsKeySync() const { return m_keySync; }

		/**
		 * \return Rate in hertz
		 */
		[[nodiscard]] double GetRate() const { return m_rate; }
		
		/**
		 * \return Delay in seconds
		 */
		[[nodiscard]] double GetDelay() const { return m_delay; }
		
		/**
		 * \return Variation percentage
		 */
		[[nodiscard]] float GetVariation() const { return m_variationPercent; }
		
	private:
		double m_rate = 0.08;
		E4LFOShape m_shape = E4LFOShape::TRIANGLE;
		double m_delay = 0.0;
		float m_variationPercent = 0.f;
		bool m_keySync = false;
	};
	
	enum struct EEOSCordSource final : uint8_t
	{
		SRC_OFF = 0ui8, XFADE_RANDOM = 4ui8, KEY_POLARITY_POS = 8ui8, KEY_POLARITY_CENTER = 9ui8,
		VEL_POLARITY_POS = 10ui8, VEL_POLARITY_CENTER = 11ui8, VEL_POLARITY_LESS = 12ui8, RELEASE_VEL = 13ui8,
		GATE = 14ui8, PITCH_WHEEL = 16ui8, MOD_WHEEL = 17ui8, PRESSURE = 18ui8, PEDAL = 19ui8,
		MIDI_A = 20ui8, MIDI_B = 21ui8, FOOTSWITCH_1 = 22ui8, FOOTSWITCH_2 = 23ui8,
		FOOTSWITCH_1_FF = 24ui8, FOOTSWITCH_2_FF = 25ui8, MIDI_VOLUME = 26ui8, MIDI_PAN = 27ui8,
		EXPRESSION = 28ui8, MIDI_C = 32ui8, MIDI_D = 33ui8, MIDI_E = 34ui8, MIDI_F = 35ui8,
		MIDI_G = 36ui8, MIDI_H = 37ui8, T_SWITCH = 38ui8, T_SWITCH_FF = 39ui8, MIDI_I = 40ui8,
		MIDI_J = 41ui8, MIDI_K = 42ui8, MIDI_L = 43ui8, MIDI_M = 44ui8, MIDI_N = 45ui8,
		MIDI_O = 46ui8, MIDI_P = 47ui8, KEY_GLIDE = 48ui8, KEY_CC_WIN = 49ui8,
		AMP_ENV_POLARITY_POS = 72ui8, AMP_ENV_POLARITY_CENTER = 73ui8, AMP_ENV_POLARITY_LESS = 74ui8,
		FILTER_ENV_POLARITY_POS = 80ui8, FILTER_ENV_POLARITY_CENTER = 81ui8, FILTER_ENV_POLARITY_LESS = 82ui8,
		AUX_ENV_POLARITY_POS = 88ui8, AUX_ENV_POLARITY_CENTER = 89ui8, AUX_ENV_POLARITY_LESS = 90ui8,
		LFO1_POLARITY_CENTER = 96ui8, LFO1_POLARITY_POS = 97ui8, WHITE_NOISE = 98ui8, PINK_NOISE = 99ui8,
		KEY_RANDOM_1 = 100ui8, KEY_RANDOM_2 = 101ui8, LFO2_POLARITY_CENTER = 104ui8, LFO2_POLARITY_POS = 105ui8,
		LAG_1_IN = 106ui8, LAG_1 = 107ui8, LAG_2_IN = 108ui8, LAG_2 = 109ui8,
		CHANNEL_LAG_1 = 128ui8, CHANNEL_RAMP = 129ui8, CHANNEL_LAG_2 = 130ui8, POLY_KEY_TIMER = 131ui8,
		CLK_2X_WHOLE_NOTE = 144ui8, CLK_WHOLE_NOTE = 145ui8, CLK_HALF_NOTE = 146ui8, CLK_QUARTER_NOTE = 147ui8,
		CLK_8TH_NOTE = 148ui8, CLK_16TH_NOTE = 149ui8, CLK_4X_WHOLE_NOTE = 150ui8, CLK_8X_WHOLE_NOTE = 151ui8,
		DC_OFFSET = 160ui8, SUMMING_AMP = 161ui8, SWITCH = 162ui8, ABSOLUTE_VALUE = 163ui8, DIODE = 164ui8,
		FLIP_FLOP = 165ui8, QUANTIZER = 166ui8, GAIN_4X = 167ui8, FUNC_GEN_1_POS = 208ui8, FUNC_GEN_1_CENTER = 209ui8,
		FUNC_GEN_1_LESS = 210ui8, FUNC_GEN_1_TRIGGER = 211ui8, FUNC_GEN_1_GATE = 212ui8,
		FUNC_GEN_2_POS = 213ui8, FUNC_GEN_2_CENTER = 214ui8, FUNC_GEN_2_LESS = 215ui8, FUNC_GEN_2_TRIGGER = 216ui8,
		FUNC_GEN_2_GATE = 217ui8, FUNC_GEN_3_POS = 218ui8, FUNC_GEN_3_CENTER = 219ui8, FUNC_GEN_3_LESS = 220ui8,
		FUNC_GEN_3_TRIGGER = 221ui8, FUNC_GEN_3_GATE = 222ui8
	};

	enum struct EEOSCordDest final : uint8_t
	{
		DST_OFF = 0ui8, KEY_SUSTAIN = 8ui8, LOOP_SELECT_CONT = 16ui8, LOOP_SELECT_JUMP = 17ui8,
		FINE_PITCH = 47ui8, PITCH = 48ui8, GLIDE_RATE = 49ui8, CHORUS_AMT = 50ui8,
		CHORUS_INITIAL = 51ui8, SAMPLE_START = 52ui8, SAMPLE_LOOP = 53ui8, SAMPLE_RETRIGGER_NEG = 54ui8,
		OSC_SPEED = 55ui8, FILTER_FREQ = 56ui8, FILTER_RES = 57ui8, REALTIME_RES = 58ui8,
		SAMPLE_RETRIGGER_POS = 59ui8, AMP_VOLUME = 64ui8, AMP_PAN = 65ui8, AMP_CROSSFADE = 66ui8,
		SEND_MAIN = 68ui8, SEND_AUX_1 = 69ui8, SEND_AUX_2 = 70ui8, SEND_AUX_3 = 71ui8,
		AMP_ENV_RATES = 72ui8, AMP_ENV_ATTACK = 73ui8, AMP_ENV_DECAY = 74ui8, AMP_ENV_RELEASE = 75ui8,
		AMP_ENV_SUSTAIN = 76ui8, FILTER_ENV_RATES = 80ui8, FILTER_ENV_ATTACK = 81ui8, FILTER_ENV_DECAY = 82ui8,
		FILTER_ENV_RELEASE = 83ui8, FILTER_ENV_SUSTAIN = 84ui8, FILTER_ENV_TRIGGER = 86ui8,
		AUX_ENV_RATES = 88ui8, AUX_ENV_ATTACK = 89ui8, AUX_ENV_DECAY = 90ui8, AUX_ENV_RELEASE = 91ui8,
		AUX_ENV_SUSTAIN = 92ui8, AUX_ENV_TRIGGER = 94ui8, LFO_1_FREQ = 96ui8, LFO_1_TRIG = 97ui8,
		LFO_2_FREQ = 104ui8, LFO_2_TRIG = 105ui8, LAG_1_IN = 106ui8, LAG_2_IN = 108ui8,
		LAG_1_RATE = 109ui8, LAG_2_RATE = 110ui8, FUNC_GEN_1_RATE = 112ui8, FUNC_GEN_1_RETRIGGER = 113ui8,
		FUNC_GEN_1_LENGTH = 114ui8, FUNC_GEN_1_DIRECTION = 115ui8, FUNC_GEN_2_RATE = 117ui8,
		FUNC_GEN_2_RETRIGGER = 118ui8, FUNC_GEN_2_LENGTH = 119ui8, FUNC_GEN_2_DIRECTION = 120ui8,
		FUNC_GEN_3_RATE = 122ui8, FUNC_GEN_3_RETRIGGER = 123ui8, FUNC_GEN_3_LENGTH = 124ui8,
		FUNC_GEN_3_DIRECTION = 125ui8, KEY_TIMER_RATE = 132ui8, WET_DRY_MIX = 144ui8, SUMMING_AMP = 161ui8,
		SWITCH = 162ui8, ABSOLUTE_VALUE = 163ui8, DIODE = 164ui8, QUANTIZER = 165ui8, FLIP_FLOP = 166ui8,
		GAIN_4X = 167ui8, CORD_1_AMT = 168ui8, CORD_2_AMT = 169ui8, CORD_3_AMT = 170ui8, CORD_4_AMT = 171ui8,
		CORD_5_AMT = 172ui8, CORD_6_AMT = 173ui8, CORD_7_AMT = 174ui8, CORD_8_AMT = 175ui8,
		CORD_9_AMT = 176ui8, CORD_10_AMT = 177ui8, CORD_11_AMT = 178ui8, CORD_12_AMT = 179ui8,
		CORD_13_AMT = 180ui8, CORD_14_AMT = 181ui8, CORD_15_AMT = 182ui8, CORD_16_AMT = 183ui8,
		CORD_17_AMT = 184ui8, CORD_18_AMT = 185ui8, CORD_19_AMT = 186ui8, CORD_20_AMT = 187ui8,
		CORD_21_AMT = 188ui8, CORD_22_AMT = 189ui8, CORD_23_AMT = 190ui8, CORD_24_AMT = 191ui8,
		CORD_25_AMT = 192ui8, CORD_26_AMT = 193ui8, CORD_27_AMT = 194ui8, CORD_28_AMT = 195ui8,
		CORD_29_AMT = 196ui8, CORD_30_AMT = 197ui8, CORD_31_AMT = 198ui8, CORD_32_AMT = 199ui8,
		CORD_33_AMT = 200ui8, CORD_34_AMT = 201ui8, CORD_35_AMT = 202ui8, CORD_36_AMT = 203ui8
	};

	struct E4Cord final
	{
		E4Cord() = default;
		
		explicit E4Cord(const EEOSCordSource src, const EEOSCordDest dst, const float amt) : m_src(src), m_dst(dst), m_percent(amt) {}

		void Write(FORMChunk& presetChunk) const
		{
			presetChunk.writeType(reinterpret_cast<const char*>(&m_src), sizeof(EEOSCordSource));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_dst), sizeof(EEOSCordDest));

			const int8_t amt(unit_helpers::ConvertPercentToByteF(std::clamp(m_percent, -100.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&amt), sizeof(int8_t));

			constexpr uint8_t unknown(0ui8);
			presetChunk.writeType(reinterpret_cast<const char*>(&unknown), sizeof(uint8_t));
		}
		
		void Read(std::ifstream& stream)
		{
			stream.read(reinterpret_cast<char*>(&m_src), sizeof(EEOSCordSource));
			stream.read(reinterpret_cast<char*>(&m_dst), sizeof(EEOSCordDest));

			int8_t amt;
			stream.read(reinterpret_cast<char*>(&amt), sizeof(int8_t));

			m_percent = unit_helpers::ConvertByteToPercentF(amt);
			
			stream.ignore(1);
		}

		void SetSrc(const EEOSCordSource src) { m_src = src; }
		void SetDst(const EEOSCordDest dst) { m_dst = dst; }
		void SetPercent(const float percent) { m_percent = std::clamp(percent, -100.f, 100.f); }

		[[nodiscard]] EEOSCordSource GetSrc() const { return m_src; }
		[[nodiscard]] EEOSCordDest GetDst() const { return m_dst; }
		[[nodiscard]] float GetPercent() const { return m_percent; }

	private:
		EEOSCordSource m_src = EEOSCordSource::SRC_OFF;
		EEOSCordDest m_dst = EEOSCordDest::DST_OFF;
		float m_percent = 0.f;
	};
	
	enum struct EEOSFilterType final : uint8_t
	{
		TWO_POLE_LOWPASS = 1ui8, FOUR_POLE_LOWPASS = 0ui8, SIX_POLE_LOWPASS = 2ui8,
		TWO_POLE_HIGHPASS = 8ui8, FOUR_POLE_HIGHPASS = 9ui8, CONTRARY_BANDPASS = 18ui8,
		SWEPT_EQ_1_OCTAVE = 32ui8, SWEPT_EQ_2_1_OCTAVE = 33ui8, SWEPT_EQ_3_1_OCTAVE = 34ui8,
		PHASER_1 = 64ui8, PHASER_2 = 65ui8, BAT_PHASER = 66ui8, FLANGER_LITE = 72ui8,
		VOCAL_AH_AY_EE = 80ui8, VOCAL_OO_AH = 81ui8, DUAL_EQ_MORPH = 96ui8,
		DUAL_EQ_LP_MORPH = 97ui8, DUAL_EQ_MORPH_EXPRESSION = 98ui8, PEAK_SHELF_MORPH = 104ui8,
		MORPH_DESIGNER = 108ui8, NO_FILTER = 127ui8, ACE_OF_BASS = 131ui8, MEGASWEEPZ = 132ui8,
		EARLY_RIZER = 133ui8, MILLENNIUM = 134ui8, MEATY_GIZMO = 135ui8, KLUB_KLASSIK = 136ui8,
		BASSBOX_303 = 137ui8, FUZZI_FACE = 138ui8, DEAD_RINGER = 139ui8, TB_OR_NOT_TB = 140ui8,
		OOH_TO_EEE = 141ui8, BOLAND_BASS = 142ui8, MULTI_Q_VOX = 143ui8, TALKING_HEDZ = 144ui8,
		ZOOM_PEAKS = 145ui8, DJ_ALKALINE = 146ui8, BASS_TRACER = 147ui8, ROGUE_HERTZ = 148ui8,
		RAZOR_BLADES = 149ui8, RADIO_CRAZE = 150ui8, EEH_TO_AAH = 151ui8, UBU_ORATOR = 152ui8,
		DEEP_BOUCHE = 153ui8, FREAK_SHIFTA = 154ui8, CRUZ_PUSHER = 155ui8, ANGELZ_HAIRZ = 156ui8,
		DREAM_WEAVA = 157ui8, ACID_RAVAGE = 158ui8, BASS_O_MATIC = 159ui8, LUCIFERS_Q = 160ui8,
		TOOTH_COMB = 161ui8, EAR_BENDER = 162ui8, KLANG_KLING = 163ui8
	};

	enum struct EEOSGlideCurveType final : uint8_t
	{
		LINEAR, LOG_LINEAR1, LOG_LINEAR2, LOG_LINEAR3, LOG_LINEAR4,
		LOG_LINEAR5, LOG_LINEAR6, LOG_LINEAR7, LOGARITHMIC
	};

	enum struct EEOSAssignGroup final : uint8_t
	{
		POLY_ALL, POLY16_A, POLY16_B, POLY8_A, POLY8_B,
		POLY8_C, POLY8_D, POLY4_A, POLY4_B, POLY4_C, POLY4_D,
		POLY2_A, POLY2_B, POLY2_C, POLY2_D, MONO_A, MONO_B,
		MONO_C, MONO_D, MONO_E, MONO_F, MONO_G, MONO_H, MONO_I,
		POLY_KEY_8_A, POLY_KEY_8_B, POLY_KEY_8_C, POLY_KEY_8_D,
		POLY_KEY_6_A, POLY_KEY_6_B, POLY_KEY_6_C, POLY_KEY_6_D,
		POLY_KEY_5_A, POLY_KEY_5_B, POLY_KEY_5_C, POLY_KEY_5_D,
		POLY_KEY_4_A, POLY_KEY_4_B, POLY_KEY_4_C, POLY_KEY_4_D,
		POLY_KEY_3_A, POLY_KEY_3_B, POLY_KEY_3_C, POLY_KEY_3_D,
		POLY_KEY_2_A, POLY_KEY_2_B, POLY_KEY_2_C, POLY_KEY_2_D,
		POLY_KEY_1_A, POLY_KEY_1_B, POLY_KEY_1_C, POLY_KEY_1_D,
	};

	enum struct EEOSKeyMode final : uint8_t
	{
		POLY_NORMAL, SOLO_MULTI_TRIGGER, SOLO_MELODY_LAST, SOLO_MELODY_LOW,
		SOLO_MELODY_HIGH, SOLO_SYNTH_LAST, SOLO_SYNTH_LOW, SOLO_SYNTH_HIGH,
		SOLO_FINGERED_GLIDE, POLY_REL_TRIG_REL_VEL, POLY_REL_TRIG_NOTE_VEL,
		SOLO_REL_TRIG_REL_VEL, SOLO_REL_TRIG_NOTE_VEL, POLY_REL_TRIG_REL_VEL_2,
		POLY_REL_TRIG_NOTE_VEL_2
	};

	struct E4Voice final
	{
		E4Voice() = default;

		void Write(FORMChunk& presetChunk) const
		{
			const uint16_t voiceDataSize(byteswap_helpers::byteswap_uint16(static_cast<uint16_t>(284 + 22 * m_zones.size())));
			presetChunk.writeType(reinterpret_cast<const char*>(&voiceDataSize), sizeof(uint16_t));

			const uint8_t zoneCount(static_cast<uint8_t>(m_zones.size()));
			presetChunk.writeType(reinterpret_cast<const char*>(&zoneCount), sizeof(uint8_t));
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_group), sizeof(uint8_t));

			const char* null(nullptr);
			presetChunk.writeType(null, 8);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_keyData), sizeof(E4SampleZoneNoteData));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_velData), sizeof(E4SampleZoneNoteData));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_rtData), sizeof(E4SampleZoneNoteData));

			presetChunk.writeType(null, 1);
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_keyAssignGroup), sizeof(EEOSAssignGroup));
			
			const uint16_t keyDelay(byteswap_helpers::byteswap_uint16(m_keyDelay));
			presetChunk.writeType(reinterpret_cast<const char*>(&keyDelay), sizeof(uint16_t));

			presetChunk.writeType(null, 3);

			const uint8_t sampleOffset(unit_helpers::ConvertPercentToByteF(std::clamp(m_sampleOffset, 0.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&sampleOffset), sizeof(uint8_t));
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_transpose), sizeof(int8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_coarseTune), sizeof(int8_t));

			const int8_t fineTune(unit_helpers::ConvertFineTuneToByte(m_fineTune));
			presetChunk.writeType(reinterpret_cast<const char*>(&fineTune), sizeof(int8_t));
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_glideRate), sizeof(uint8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_fixedPitch), sizeof(bool));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_keyMode), sizeof(EEOSKeyMode));

			presetChunk.writeType(null, 1);

			const uint8_t chorusWidth(unit_helpers::ConvertChorusWidthToByte(std::clamp(m_chorusWidth, 0.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&chorusWidth), sizeof(uint8_t));

			const uint8_t chorusAmount(unit_helpers::ConvertPercentToByteF(std::clamp(m_chorusAmount, 0.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&chorusAmount), sizeof(uint8_t));

			presetChunk.writeType(null, 7);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_keyLatch), sizeof(bool));

			presetChunk.writeType(null, 2);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_glideCurveType), sizeof(EEOSGlideCurveType));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_volume), sizeof(int8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_pan), sizeof(int8_t));
			
			presetChunk.writeType(null, 1);
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_ampEnvDynRange), sizeof(int8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_filterType), sizeof(EEOSFilterType));
			
			presetChunk.writeType(null, 1);

			const uint8_t filterFrequency(unit_helpers::ConvertFilterFrequencyToByte(std::clamp(m_filterFrequency, 57ui16, 20000ui16)));
			presetChunk.writeType(reinterpret_cast<const char*>(&filterFrequency), sizeof(uint8_t));

			const uint8_t filterResonance(unit_helpers::ConvertPercentToByteF(std::clamp(m_filterResonance, 0.f, 100.f)));
			presetChunk.writeType(reinterpret_cast<const char*>(&filterResonance), sizeof(uint8_t));

			presetChunk.writeType(null, 48);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_ampEnv), sizeof(E4Envelope));

			presetChunk.writeType(null, 2);
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_filterEnv), sizeof(E4Envelope));

			presetChunk.writeType(null, 2);
			
			presetChunk.writeType(reinterpret_cast<const char*>(&m_auxEnv), sizeof(E4Envelope));

			presetChunk.writeType(null, 2);

			m_lfo1.Write(presetChunk);

			presetChunk.writeType(null, 1);
			
			m_lfo2.Write(presetChunk);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_lfoLag1), sizeof(uint8_t));

			presetChunk.writeType(null, 1);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_lfoLag2), sizeof(uint8_t));

			presetChunk.writeType(null, 20);

			for(const auto& cord : m_cords)
			{
				cord.Write(presetChunk);
			}

			for(const auto& zone : m_zones)
			{
				zone.Write(presetChunk);
			}
		}
		
		void Read(std::ifstream& stream)
		{
			uint16_t voiceDataSize(0ui16);
			stream.read(reinterpret_cast<char*>(&voiceDataSize), sizeof(uint16_t));
			voiceDataSize = byteswap_helpers::byteswap_uint16(voiceDataSize);
			
			assert(voiceDataSize % 22 == 20);
			if(voiceDataSize % 22 != 20)
			{
				return;
			}

			uint8_t zoneCount(0ui8);
			stream.read(reinterpret_cast<char*>(&zoneCount), sizeof(uint8_t));

			assert(zoneCount > 0ui8);
			if(zoneCount == 0ui8)
			{
				return;
			}
			
			stream.read(reinterpret_cast<char*>(&m_group), sizeof(uint8_t));

			stream.ignore(8);

			stream.read(reinterpret_cast<char*>(&m_keyData), sizeof(E4SampleZoneNoteData));
			stream.read(reinterpret_cast<char*>(&m_velData), sizeof(E4SampleZoneNoteData));
			stream.read(reinterpret_cast<char*>(&m_rtData), sizeof(E4SampleZoneNoteData));

			stream.ignore(1);
			
			stream.read(reinterpret_cast<char*>(&m_keyAssignGroup), sizeof(EEOSAssignGroup));
			
			stream.read(reinterpret_cast<char*>(&m_keyDelay), sizeof(uint16_t));
			m_keyDelay = byteswap_helpers::byteswap_uint16(m_keyDelay);

			stream.ignore(3);

			uint8_t sampleOffset;
			stream.read(reinterpret_cast<char*>(&sampleOffset), sizeof(uint8_t));

			m_sampleOffset = unit_helpers::ConvertByteToPercentF(sampleOffset);
			
			stream.read(reinterpret_cast<char*>(&m_transpose), sizeof(int8_t));
			stream.read(reinterpret_cast<char*>(&m_coarseTune), sizeof(int8_t));

			int8_t fineTune;
			stream.read(reinterpret_cast<char*>(&fineTune), sizeof(int8_t));

			m_fineTune = unit_helpers::ConvertByteToFineTune(fineTune);
			
			stream.read(reinterpret_cast<char*>(&m_glideRate), sizeof(uint8_t));
			stream.read(reinterpret_cast<char*>(&m_fixedPitch), sizeof(bool));
			stream.read(reinterpret_cast<char*>(&m_keyMode), sizeof(EEOSKeyMode));

			stream.ignore(1);

			uint8_t chorusWidth;
			stream.read(reinterpret_cast<char*>(&chorusWidth), sizeof(uint8_t));

			m_chorusWidth = unit_helpers::GetChorusWidthPercent(chorusWidth);

			uint8_t chorusAmt;
			stream.read(reinterpret_cast<char*>(&chorusAmt), sizeof(uint8_t));

			m_chorusAmount = unit_helpers::round_f_places(unit_helpers::ConvertByteToPercentF(chorusAmt), 2u);

			stream.ignore(1);

			stream.read(reinterpret_cast<char*>(&m_chorusInitItd), sizeof(uint8_t));
			
			stream.ignore(5);

			stream.read(reinterpret_cast<char*>(&m_keyLatch), sizeof(bool));

			stream.ignore(2);

			stream.read(reinterpret_cast<char*>(&m_glideCurveType), sizeof(EEOSGlideCurveType));
			stream.read(reinterpret_cast<char*>(&m_volume), sizeof(int8_t));
			stream.read(reinterpret_cast<char*>(&m_pan), sizeof(int8_t));
			
			stream.ignore(1);
			
			stream.read(reinterpret_cast<char*>(&m_ampEnvDynRange), sizeof(int8_t));
			stream.read(reinterpret_cast<char*>(&m_filterType), sizeof(EEOSFilterType));
			
			stream.ignore(1);

			uint8_t filterFreq;
			stream.read(reinterpret_cast<char*>(&filterFreq), sizeof(uint8_t));

			m_filterFrequency = unit_helpers::ConvertByteToFilterFrequency(filterFreq);
			
			uint8_t filterRes;
			stream.read(reinterpret_cast<char*>(&filterRes), sizeof(uint8_t));

			m_filterResonance = unit_helpers::round_f_places(unit_helpers::ConvertByteToPercentF(filterRes), 1u);

			stream.ignore(48);

			stream.read(reinterpret_cast<char*>(&m_ampEnv), sizeof(E4Envelope));

			stream.ignore(2);
			
			stream.read(reinterpret_cast<char*>(&m_filterEnv), sizeof(E4Envelope));

			stream.ignore(2);
			
			stream.read(reinterpret_cast<char*>(&m_auxEnv), sizeof(E4Envelope));

			stream.ignore(2);

			m_lfo1.Read(stream);

			stream.ignore(1);
			
			m_lfo2.Read(stream);

			stream.read(reinterpret_cast<char*>(&m_lfoLag1), sizeof(uint8_t));
			
			stream.ignore(1);
			
			stream.read(reinterpret_cast<char*>(&m_lfoLag2), sizeof(uint8_t));

			stream.ignore(20);

			for(auto& cord : m_cords)
			{
				cord.Read(stream);
			}

			for(uint8_t i(0ui8); i < zoneCount; ++i)
			{
				E4SampleZone zone;
				zone.Read(stream);

				m_zones.emplace_back(std::move(zone));
			}
		}

		[[nodiscard]] bool GetPercentFromCord(const EEOSCordSource src, const EEOSCordDest dst, float& outPercent) const
		{
			for(const auto& cord : m_cords)
			{
				if(cord.GetSrc() == src && cord.GetDst() == dst)
				{
					outPercent = cord.GetPercent();
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] bool HasCord(const EEOSCordSource src) const
		{
			for(const auto& cord : m_cords) { if(cord.GetSrc() == src) { return true; } }
			return false;
		}

		void ReplaceOrAddCord(const E4Cord& cord)
		{
			// Replace if existing
			for(auto& existingCord : m_cords)
			{
				if(existingCord.GetSrc() == cord.GetSrc() && existingCord.GetDst() == cord.GetDst())
				{
					existingCord.SetPercent(cord.GetPercent());
					return;
				}
			}

			// Replace null cord
			for(auto& existingCord : m_cords)
			{
				if(existingCord.GetSrc() == EEOSCordSource::SRC_OFF && existingCord.GetDst() == EEOSCordDest::DST_OFF)
				{
					existingCord = cord;
					break;
				}
			}
		}

		void SetGroup(const uint8_t group) { m_group = std::clamp(group, 0ui8, 31ui8); }
		void SetKeyData(const E4SampleZoneNoteData data) { m_keyData = data; }
		
		void SetVelData(const E4SampleZoneNoteData data) { m_velData = data; }
		void SetRTData(const E4SampleZoneNoteData data) { m_rtData = data; }
		void SetKeyAssignGroup(const EEOSAssignGroup group) { m_keyAssignGroup = group; }
		void SetKeyDelay(const uint16_t milliseconds) { m_keyDelay = std::clamp(milliseconds, 0ui16, 10000ui16); }
		void SetSampleOffset(const float offset) { m_sampleOffset = offset; }
		void SetTranspose(const int8_t cents) { m_transpose = std::clamp(cents, MIN_TRANSPOSE_BYTE, MAX_TRANSPOSE_BYTE); }
		void SetCoarseTune(const int8_t cents) { m_coarseTune = std::clamp(cents, MIN_COARSE_TUNE_BYTE, MAX_COARSE_TUNE_BYTE); }
		void SetFineTune(const double fineTune) { m_fineTune = std::clamp(fineTune, -100.0, 100.0); }
		void SetIsFixedPitch(const bool arg) { m_fixedPitch = arg; }
		void SetKeyMode(const EEOSKeyMode mode) { m_keyMode = mode; }
		void SetChorusWidth(const float percent) { m_chorusWidth = std::clamp(percent, 0.f, 100.f); }
		void SetChorusAmount(const float percent) { m_chorusAmount = std::clamp(percent, 0.f, 100.f); }
		void SetGlideCurveType(const EEOSGlideCurveType type) { m_glideCurveType = type; }
		void SetVolume(const int8_t dB) { m_volume = std::clamp(dB, MIN_VOLUME_BYTE, MAX_VOLUME_BYTE); }
		void SetPan(const int8_t pan) { m_pan = std::clamp(pan, MIN_PAN_BYTE, MAX_PAN_BYTE); }
		void SetFilterType(const EEOSFilterType type) { m_filterType = type; }
		void SetFilterFrequency(const uint16_t hertz) { m_filterFrequency = std::clamp(hertz, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY); }
		void SetFilterResonance(const float percent) { m_filterResonance = std::clamp(percent, 0.f, 100.f); }
		void SetLFOLag1(const uint8_t lag) { m_lfoLag1 = std::clamp(lag, MIN_LFO_LAG_BYTE, MAX_LFO_LAG_BYTE); }
		void SetLFOLag2(const uint8_t lag) { m_lfoLag2 = std::clamp(lag, MIN_LFO_LAG_BYTE, MAX_LFO_LAG_BYTE); }

		void AddSampleZone(E4SampleZone&& zone)
		{
			assert(m_zones.size() < EOS_E4_MAX_ZONES);
			if(m_zones.size() < EOS_E4_MAX_ZONES)
			{
				m_zones.emplace_back(std::move(zone));	
			}
		}

		void RemoveSampleZone(const std::vector<E4SampleZone>::const_iterator& iter)
		{
			m_zones.erase(iter);
		}

		/**
		 * \return Volume in dB
		 */
		[[nodiscard]] int8_t GetVolume() const { return m_volume; }

		/**
		 * \return Dynamic range in dB
		 */
		[[nodiscard]] int8_t GetAmpEnvDynRange() const { return m_ampEnvDynRange; }

		/**
		 * \return Filter frequency in hertz
		 */
		[[nodiscard]] uint16_t GetFilterFrequency() const { return m_filterFrequency; }
		
		/**
		 * \return Resonance percentage
		 */
		[[nodiscard]] float GetFilterResonance() const { return m_filterResonance; }

		/**
		 * \return Delay in milliseconds
		 */
		[[nodiscard]] uint16_t GetKeyDelay() const { return m_keyDelay; }

		/**
		 * \return Chorus amount percentage
		 */
		[[nodiscard]] float GetChorusAmount() const { return m_chorusAmount; }

		/**
		 * \brief 
		 * \return Chorus width percentage
		 */
		[[nodiscard]] float GetChorusWidth() const { return m_chorusWidth; }

		/**
		* \return Chorus Inter-Aural time delay in milliseconds
		 */
		[[nodiscard]] uint8_t GetChorusInitItd() const { return m_chorusInitItd; }

		/**
		 * \return Glide rate in seconds
		 */
		[[nodiscard]] uint8_t GetGlideRate() const { return m_glideRate; }
		
		[[nodiscard]] uint8_t GetGroup() const { return m_group; }
		[[nodiscard]] E4SampleZoneNoteData& GetKeyData() { return m_keyData; }
		[[nodiscard]] E4SampleZoneNoteData& GetVelData() { return m_velData; }
		[[nodiscard]] E4SampleZoneNoteData& GetRTData() { return m_rtData; }
		[[nodiscard]] const E4SampleZoneNoteData& GetKeyData() const { return m_keyData; }
		[[nodiscard]] const E4SampleZoneNoteData& GetVelData() const { return m_velData; }
		[[nodiscard]] const E4SampleZoneNoteData& GetRTData() const { return m_rtData; }
		[[nodiscard]] EEOSAssignGroup GetKeyAssignGroup() const { return m_keyAssignGroup; }
		[[nodiscard]] float GetSampleOffset() const { return m_sampleOffset; }
		[[nodiscard]] int8_t GetTranspose() const { return m_transpose; }
		[[nodiscard]] int8_t GetCoarseTune() const { return m_coarseTune; }
		[[nodiscard]] double GetFineTune() const { return m_fineTune; }
		[[nodiscard]] bool IsFixedPitch() const { return m_fixedPitch; }
		[[nodiscard]] EEOSKeyMode GetKeyMode() const { return m_keyMode; }
		[[nodiscard]] bool IsKeyLatch() const { return m_keyLatch; }
		[[nodiscard]] EEOSGlideCurveType GetGlideCurveType() const { return m_glideCurveType; }
		[[nodiscard]] int8_t GetPan() const { return m_pan; }
		[[nodiscard]] EEOSFilterType GetFilterType() const { return m_filterType; }
		[[nodiscard]] E4Envelope& GetAmpEnv() { return m_ampEnv; }
		[[nodiscard]] E4Envelope& GetFilterEnv() { return m_filterEnv; }
		[[nodiscard]] E4Envelope& GetAuxEnv() { return m_auxEnv; }
		[[nodiscard]] const E4Envelope& GetAmpEnv() const { return m_ampEnv; }
		[[nodiscard]] const E4Envelope& GetFilterEnv() const { return m_filterEnv; }
		[[nodiscard]] const E4Envelope& GetAuxEnv() const { return m_auxEnv; }
		[[nodiscard]] E4LFO& GetLFO1() { return m_lfo1; }
		[[nodiscard]] E4LFO& GetLFO2() { return m_lfo2; }
		[[nodiscard]] const E4LFO& GetLFO1() const { return m_lfo1; }
		[[nodiscard]] const E4LFO& GetLFO2() const { return m_lfo2; }
		[[nodiscard]] uint8_t GetLFOLag1() const { return m_lfoLag1; }
		[[nodiscard]] uint8_t GetLFOLag2() const { return m_lfoLag2; }
		[[nodiscard]] std::array<E4Cord, 24>& GetCords() { return m_cords; }
		[[nodiscard]] const std::vector<E4SampleZone>& GetSampleZones() const { return m_zones; }
		
	private:
		uint8_t m_group = 0ui8; // [0 (1), 31 (32)]
		std::array<int8_t, 8> m_amplifierData{'\0', 100i8};

		E4SampleZoneNoteData m_keyData;
		E4SampleZoneNoteData m_velData;
		E4SampleZoneNoteData m_rtData;
		
		EEOSAssignGroup m_keyAssignGroup = EEOSAssignGroup::POLY_ALL;
		uint16_t m_keyDelay = 0ui16; // [0, 10000]
		float m_sampleOffset = 0.f;

		int8_t m_transpose = 0i8; // [-36, 36]
		int8_t m_coarseTune = 0i8; // [-72, 24]
		double m_fineTune = 0.0;
		uint8_t m_glideRate = 0ui8; // [0 (0 sec), 127 (32.737 sec)] // TODO: Convert to double
		bool m_fixedPitch = false;
		EEOSKeyMode m_keyMode = EEOSKeyMode::POLY_NORMAL;
		float m_chorusWidth = 100.f;
		
		float m_chorusAmount = 0.f;
		uint8_t m_chorusInitItd = 0ui8; // [-32 (-1.45), 32 (1.45)] // TODO: Convert to float
		bool m_keyLatch = false;
		EEOSGlideCurveType m_glideCurveType = EEOSGlideCurveType::LINEAR;
		int8_t m_volume = 0i8; // [-96, 10]
		int8_t m_pan = 0i8; // [-64, 63]
		int8_t m_ampEnvDynRange = 0i8; // [0 (-96), 16 (-48)]

		EEOSFilterType m_filterType = EEOSFilterType::NO_FILTER;
		uint16_t m_filterFrequency = 20000ui16;
		float m_filterResonance = 0.f;

		E4Envelope m_ampEnv{};
		E4Envelope m_filterEnv{};
		E4Envelope m_auxEnv{};

		E4LFO m_lfo1 = E4LFO(5.79, E4LFOShape::SINE, 0.0, 0.f, true);
		E4LFO m_lfo2 = E4LFO(5.79, E4LFOShape::SINE, 0.0, 0.f, true);
		uint8_t m_lfoLag1 = 0ui8; // [0, 10]
		uint8_t m_lfoLag2 = 0ui8; // [0, 10]

		std::array<E4Cord, 24> m_cords = {
			E4Cord(EEOSCordSource::VEL_POLARITY_LESS, EEOSCordDest::AMP_VOLUME, 0.f), E4Cord(EEOSCordSource::PITCH_WHEEL, EEOSCordDest::PITCH, 0.f),
			E4Cord(EEOSCordSource::LFO1_POLARITY_CENTER, EEOSCordDest::PITCH, 0.f), E4Cord(EEOSCordSource::MOD_WHEEL, EEOSCordDest::CORD_3_AMT, 6.f),
			E4Cord(EEOSCordSource::VEL_POLARITY_LESS, EEOSCordDest::FILTER_FREQ, 0.f), E4Cord(EEOSCordSource::FILTER_ENV_POLARITY_POS, EEOSCordDest::FILTER_FREQ, 0.f),
			E4Cord(EEOSCordSource::KEY_POLARITY_CENTER, EEOSCordDest::FILTER_FREQ, 0.f), E4Cord(EEOSCordSource::FOOTSWITCH_1, EEOSCordDest::KEY_SUSTAIN, 100.f)
		};

		/*
		 * Allocated data
		 */
		
		std::vector<E4SampleZone> m_zones{};
	};

	/*
	 * Presets:
	 */

	struct E4Preset final
	{
		E4Preset() = default;
		
		explicit E4Preset(std::string&& presetName, std::vector<E4Voice>&& voices, const uint16_t index = std::numeric_limits<uint16_t>::max()) : m_voices(std::move(voices))
		{
			SetIndex(index);
			
			ApplyEOSNamingStandards(presetName);
			m_name = std::move(presetName);
		}
		
		void Write(FORMChunk& presetChunk) const
		{
			const uint16_t index(byteswap_helpers::byteswap_uint16(m_index));
			presetChunk.writeType(reinterpret_cast<const char*>(&index), sizeof(uint16_t));
			
			presetChunk.writeType(m_name.data(), EOS_E4_MAX_NAME_LEN);

			constexpr uint16_t unknown(byteswap_helpers::byteswap_uint16(82ui16));
			presetChunk.writeType(reinterpret_cast<const char*>(&unknown), sizeof(uint16_t));

			const uint16_t numVoices(byteswap_helpers::byteswap_uint16(static_cast<uint16_t>(m_voices.size())));
			presetChunk.writeType(reinterpret_cast<const char*>(&numVoices), sizeof(uint16_t));

			const char* null(nullptr);
			presetChunk.writeType(null, 4);

			presetChunk.writeType(reinterpret_cast<const char*>(&m_transpose), sizeof(int8_t));
			presetChunk.writeType(reinterpret_cast<const char*>(&m_volume), sizeof(int8_t));

			presetChunk.writeType(null, 24);

			constexpr std::array unknown2{'R', '#', '\0', '~'};
			presetChunk.writeType(unknown2.data(), unknown2.size());

			presetChunk.writeType(reinterpret_cast<const char*>(m_initialMIDIControllers.data()), static_cast<std::streamsize>(m_initialMIDIControllers.size()));

			presetChunk.writeType(null, 24);

			for(const auto& voice : m_voices)
			{
				voice.Write(presetChunk);
			}
		}
		
		void Read(std::ifstream& stream)
		{
			stream.read(reinterpret_cast<char*>(&m_index), sizeof(uint16_t));
			m_index = byteswap_helpers::byteswap_uint16(m_index);

			m_name.clear();
			m_name.resize(EOS_E4_MAX_NAME_LEN);
			stream.read(m_name.data(), EOS_E4_MAX_NAME_LEN);

			uint16_t unknown;
			stream.read(reinterpret_cast<char*>(&unknown), sizeof(uint16_t));
			unknown = byteswap_helpers::byteswap_uint16(unknown);

			// Data size is always generally 82, ensure this.
			if(unknown != 82ui16)
			{
				assert(unknown == 82ui16);
				return;
			}

			uint16_t numVoices;
			stream.read(reinterpret_cast<char*>(&numVoices), sizeof(uint16_t));
			numVoices = byteswap_helpers::byteswap_uint16(numVoices);

			stream.ignore(4);

			stream.read(reinterpret_cast<char*>(&m_transpose), sizeof(int8_t));
			stream.read(reinterpret_cast<char*>(&m_volume), sizeof(int8_t));

			stream.ignore(28);

			stream.read(reinterpret_cast<char*>(m_initialMIDIControllers.data()), static_cast<std::streamsize>(m_initialMIDIControllers.size()));

			stream.ignore(24);

			for(uint16_t i(0ui16); i < numVoices; ++i)
			{
				E4Voice voice;
				voice.Read(stream);

				m_voices.emplace_back(std::move(voice));
			}
		}

		void AddVoice(E4Voice&& voice)
		{
			assert(m_voices.size() < EOS_E4_MAX_VOICES);
			if(m_voices.size() < EOS_E4_MAX_VOICES)
			{
				m_voices.emplace_back(std::move(voice));	
			}
		}

		void RemoveVoice(const std::vector<E4Voice>::const_iterator& iter)
		{
			m_voices.erase(iter);
		}

		void SetIndex(const uint16_t index)
		{
			// Max indicates that the index will be automatically assigned.
			if(index == std::numeric_limits<uint16_t>::max()) { m_index = index; return; }
			
			m_index = std::clamp(index, 0ui16, static_cast<uint16_t>(EOS_E4_MAX_PRESETS));
		}

		void SetName(std::string&& name)
		{
			ApplyEOSNamingStandards(name);
			m_name = std::move(name);
		}

		void SetTranspose(const int8_t cents) { m_transpose = std::clamp(cents, MIN_TRANSPOSE_BYTE, MAX_TRANSPOSE_BYTE); }
		void SetVolume(const int8_t dB) { m_volume = std::clamp(dB, MIN_VOLUME_BYTE, MAX_VOLUME_BYTE); }

		[[nodiscard]] uint16_t GetIndex() const { return m_index; }
		[[nodiscard]] const std::string& GetName() const { return m_name; }
		[[nodiscard]] const std::vector<E4Voice>& GetVoices() const { return m_voices; }
		
		/**
		 * \return Transpose in cents
		 */
		[[nodiscard]] int8_t GetTranspose() const { return m_transpose; }
		
		/**
		 * \return Volume in dB
		 */
		[[nodiscard]] int8_t GetVolume() const { return m_volume; }
		
	private:
		uint16_t m_index = 0ui16; // requires byteswap
		std::string m_name;
		int8_t m_transpose = 0i8; // [-12, 12]
		int8_t m_volume = 0i8; // [-96, 10]
		std::array<uint8_t, 4> m_initialMIDIControllers{EOS_E4_INITIAL_MIDI_CONTROLLER_OFF, EOS_E4_INITIAL_MIDI_CONTROLLER_OFF,
			EOS_E4_INITIAL_MIDI_CONTROLLER_OFF, EOS_E4_INITIAL_MIDI_CONTROLLER_OFF}; // [0, 255]

		/*
		 * Allocated data:
		 */

		std::vector<E4Voice> m_voices;
	};

	/*
	 * Sample
	 */

	namespace E3SampleHelpers
	{
		constexpr auto EOS_MONO_SAMPLE_L = 0x00200000;
		constexpr auto EOS_MONO_SAMPLE_R = 0x00400000;
		constexpr auto EOS_STEREO_SAMPLE = 0x00600000;
		constexpr auto SAMPLE_LOOP_FLAG = 0x00010000;
		constexpr auto SAMPLE_RELEASE_FLAG = 0x00080000;

		inline uint32_t GetNumChannels(const uint32_t format)
		{
			if ((format & EOS_STEREO_SAMPLE) == EOS_STEREO_SAMPLE)
			{
				return 2u;	
			}

			if ((format & EOS_MONO_SAMPLE_L) == EOS_MONO_SAMPLE_L
				 || (format & EOS_MONO_SAMPLE_R) == EOS_MONO_SAMPLE_R)
			{
				return 1u;
			}
			
			return 1u;
		}
		
		inline bool IsLooping(const uint32_t format)
		{
			return (format & SAMPLE_LOOP_FLAG) == SAMPLE_LOOP_FLAG;
		}

		inline bool IsLoopingInRelease(const uint32_t format)
		{
			return (format & SAMPLE_RELEASE_FLAG) == SAMPLE_RELEASE_FLAG;
		}
	}

	struct E3SampleParams final
	{
		E3SampleParams() = default;

		explicit E3SampleParams(const uint32_t numSamples, const uint32_t numChannels, const uint32_t loopStart, const uint32_t loopEnd)
			: m_sampleStartR(numChannels == 1u ? m_sampleStartL : numSamples + 92u), m_sampleEndL(numChannels == 1u ? numSamples * sizeof(int16_t) + 92u - 2u : numSamples + 92u - 2u),
			m_sampleEndR(numChannels == 1u ? m_sampleEndL : numSamples * sizeof(int16_t) + 92u - 2u)
		{
			SetLoopStart(loopStart, numSamples, numChannels);
			SetLoopEnd(loopEnd, numSamples, numChannels);
		}
		
		void Read(std::ifstream& stream)
		{
			stream.read(reinterpret_cast<char*>(&m_unknown), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_sampleStartL), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_sampleStartR), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_sampleEndL), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_sampleEndR), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_loopStartL), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_loopStartR), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_loopEndL), sizeof(uint32_t));
			stream.read(reinterpret_cast<char*>(&m_loopEndR), sizeof(uint32_t));
		}
		
		void Write(FORMChunk& sampleChunk) const
		{
			sampleChunk.writeType(&m_unknown);
			sampleChunk.writeType(&m_sampleStartL);
			sampleChunk.writeType(&m_sampleStartR);
			sampleChunk.writeType(&m_sampleEndL);
			sampleChunk.writeType(&m_sampleEndR);
			sampleChunk.writeType(&m_loopStartL);
			sampleChunk.writeType(&m_loopStartR);
			sampleChunk.writeType(&m_loopEndL);
			sampleChunk.writeType(&m_loopEndR);
		}

		void SetLoopStart(uint32_t loopStart, const uint32_t numSamples, const uint32_t numChannels)
		{
			loopStart = std::clamp(loopStart, 0u, numSamples - 1u);
			
			m_loopStartL = loopStart * sizeof(int16_t) + m_sampleStartL;
			m_loopStartR = numChannels == 1u ? m_loopStartL : loopStart * sizeof(int16_t) + m_sampleStartR;
		}

		void SetLoopEnd(uint32_t loopEnd, const uint32_t numSamples, const uint32_t numChannels)
		{
			loopEnd = std::clamp(loopEnd, 0u, numSamples);
			
			m_loopEndL = loopEnd * sizeof(int16_t) + m_sampleStartL - 2u;
			m_loopEndR = numChannels == 1u ? m_loopEndL : loopEnd * sizeof(int16_t) + m_sampleStartR - 2u;
		}

		[[nodiscard]] uint32_t GetLoopStartL() const { return (m_loopStartL - 92u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetLoopStartR() const { return (m_loopStartR - m_sampleStartR) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetLoopEndL() const { return (m_loopEndL - 92u + 2u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetLoopEndR() const { return (m_loopEndR - m_sampleStartR + 2u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetSampleStartL() const { return (m_sampleStartL - 92u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetSampleStartR() const { return (m_sampleStartR - 92u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetSampleEndL() const { return (m_sampleEndL - 92u + 2u) / sizeof(int16_t); }
		[[nodiscard]] uint32_t GetSampleEndR() const { return (m_sampleEndR - 92u + 2u) / sizeof(int16_t); }

	private:
		uint32_t m_unknown = 0u;
		uint32_t m_sampleStartL = 92u;
		uint32_t m_sampleStartR = 92u;
		uint32_t m_sampleEndL = 0u;
		uint32_t m_sampleEndR = 0u;
		uint32_t m_loopStartL = 0u;
		uint32_t m_loopStartR = 0u;
		uint32_t m_loopEndL = 0u;
		uint32_t m_loopEndR = 0u;
	};

	enum struct ESampleType final : uint8_t
	{
		LEFT = 0ui8, MONO = 0ui8, RIGHT
	};

	struct SampleLoopInfo final
	{
		explicit SampleLoopInfo(const bool loop = false, const bool loopInRelease = false, const uint32_t loopStart = 0u, const uint32_t loopEnd = 0u)
			: m_loop(loop), m_loopInRelease(loopInRelease), m_loopStart(loopStart), m_loopEnd(loopEnd) {}
		
		bool m_loop = false;
		bool m_loopInRelease = false;
		uint32_t m_loopStart = 0u;
		uint32_t m_loopEnd = 0u;
	};
	
	struct E3Sample final
	{
		E3Sample() = default;
		
		explicit E3Sample(std::string&& sampleName, std::vector<int16_t>&& sampleData, const uint32_t sampleRate, const uint32_t numChannels,
			const SampleLoopInfo& loopInfo, const uint16_t index = std::numeric_limits<uint16_t>::max()) : m_loopInfo(loopInfo), m_sampleRate(std::clamp(sampleRate, 7000u, 192000u)),
			m_numChannels(std::clamp(numChannels, 1u, 2u)), m_sampleData(std::move(sampleData)), m_params(static_cast<uint32_t>(m_sampleData.size()), numChannels, loopInfo.m_loopStart, loopInfo.m_loopEnd)
		{
			SetIndex(index);
			
			ApplyEOSNamingStandards(sampleName);
			m_name = std::move(sampleName);
		}

		void Write(FORMChunk& sampleChunk) const
		{
			if(m_sampleData.empty())
			{
				assert(!m_sampleData.empty());
				return;
			}
			
			const uint16_t index(byteswap_helpers::byteswap_uint16(m_index));
			sampleChunk.writeType(reinterpret_cast<const char*>(&index), sizeof(uint16_t));
			
			sampleChunk.writeType(m_name.data(), EOS_E4_MAX_NAME_LEN);
			
			m_params.Write(sampleChunk);
			
			sampleChunk.writeType(&m_sampleRate);

			uint32_t format(m_numChannels == 1u ? E3SampleHelpers::EOS_MONO_SAMPLE_L : E3SampleHelpers::EOS_STEREO_SAMPLE);
			if(m_loopInfo.m_loop) { format |= E3SampleHelpers::SAMPLE_LOOP_FLAG; }
			if(m_loopInfo.m_loopInRelease) { format |= E3SampleHelpers::SAMPLE_RELEASE_FLAG; }
			
			sampleChunk.writeType(&format);

			sampleChunk.writeType(m_extraParams.data(), sizeof(uint32_t) * EOS_NUM_EXTRA_SAMPLE_PARAMETERS);
			sampleChunk.writeType(m_sampleData.data(), sizeof(uint16_t) * m_sampleData.size());
		}
		
		void Read(std::ifstream& stream, const size_t subChunkSize)
		{
			stream.read(reinterpret_cast<char*>(&m_index), sizeof(uint16_t));
			m_index = byteswap_helpers::byteswap_uint16(m_index);

			m_name.clear();
			m_name.resize(EOS_E4_MAX_NAME_LEN);
			stream.read(m_name.data(), EOS_E4_MAX_NAME_LEN);
			
			m_params.Read(stream);

			stream.read(reinterpret_cast<char*>(&m_sampleRate), sizeof(uint32_t));

			uint32_t format(0u);
			stream.read(reinterpret_cast<char*>(&format), sizeof(uint32_t));

			m_numChannels = E3SampleHelpers::GetNumChannels(format);
			m_loopInfo = SampleLoopInfo(E3SampleHelpers::IsLooping(format), E3SampleHelpers::IsLoopingInRelease(format),
				m_params.GetLoopStartL(), m_params.GetLoopEndL());
			
			stream.read(reinterpret_cast<char*>(m_extraParams.data()), sizeof(uint32_t) * EOS_NUM_EXTRA_SAMPLE_PARAMETERS);

			constexpr size_t SAMPLE_INFO_WITHOUT_SIZE(sizeof(uint16_t) + EOS_E4_MAX_NAME_LEN +
				sizeof(E3SampleParams) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) * EOS_NUM_EXTRA_SAMPLE_PARAMETERS);

			m_sampleData.clear();
			m_sampleData.resize((subChunkSize - SAMPLE_INFO_WITHOUT_SIZE) / sizeof(int16_t));
			stream.read(reinterpret_cast<char*>(m_sampleData.data()), static_cast<std::streamsize>(sizeof(int16_t) * m_sampleData.size()));
		}
		
		void SetNumChannels(const uint32_t channels) { m_numChannels = std::clamp(channels, 1u, 2u); }
		void SetSampleRate(const uint32_t sampleRate) { m_sampleRate = std::clamp(sampleRate, 7000u, 192000u); }
		void SetSampleData(std::vector<int16_t>&& data) { m_sampleData = std::move(data); }

		void SetIndex(const uint16_t index)
		{
			// Max indicates that the index will be automatically assigned.
			if(index == std::numeric_limits<uint16_t>::max()) { m_index = index; return; }
			
			m_index = std::clamp(index, 0ui16, static_cast<uint16_t>(EOS_E4_MAX_SAMPLES));
		}
		
		void SetName(std::string&& name)
		{
			ApplyEOSNamingStandards(name);
			m_name = std::move(name);
		}

		[[nodiscard]] uint16_t GetIndex() const { return m_index; }
		[[nodiscard]] uint32_t GetNumChannels() const { return m_numChannels; }
		[[nodiscard]] uint32_t GetSampleRate() const { return m_sampleRate; }
		[[nodiscard]] const std::string& GetName() const { return m_name; }
		[[nodiscard]] SampleLoopInfo& GetLoopInfo() { return m_loopInfo; }
		[[nodiscard]] const SampleLoopInfo& GetLoopInfo() const { return m_loopInfo; }

		[[nodiscard]] std::vector<int16_t> GetSampleData(const ESampleType type) const
		{
			if(type == ESampleType::RIGHT && m_numChannels == 2u)
			{
				return std::vector<int16_t>{m_sampleData.begin() + m_params.GetSampleStartR(),
					m_sampleData.end() - (static_cast<uint32_t>(m_sampleData.size()) - m_params.GetSampleEndR())};
			}
			
			return std::vector<int16_t>{m_sampleData.begin(), m_sampleData.end() - (static_cast<uint32_t>(m_sampleData.size()) - m_params.GetSampleEndL())};
		}

	private:
		uint16_t m_index = std::numeric_limits<uint16_t>::max(); // requires byteswap
		std::string m_name;
		std::array<uint32_t, EOS_NUM_EXTRA_SAMPLE_PARAMETERS> m_extraParams{}; // Always seems to be empty

		SampleLoopInfo m_loopInfo;
    
		uint32_t m_sampleRate = 0u; // [7000, 192000]
		uint32_t m_numChannels = 0u; // [1, 2]
		
		std::vector<int16_t> m_sampleData{};
		
		E3SampleParams m_params;
	};

	/*
	 * Sequences:
	 */

	struct E4Sequence final
	{
		E4Sequence() = default;

		explicit E4Sequence(std::string&& seqName, std::vector<char>&& midiData, const uint16_t index = std::numeric_limits<uint16_t>::max()) : m_midiData(std::move(midiData))
		{
			SetIndex(index);
			
			ApplyEOSNamingStandards(seqName);
			m_name = std::move(seqName);
		}

		void Write(FORMChunk& sampleChunk) const
		{
			if(m_midiData.empty())
			{
				assert(!m_midiData.empty());
				return;
			}
			
			const uint16_t index(byteswap_helpers::byteswap_uint16(m_index));
			sampleChunk.writeType(reinterpret_cast<const char*>(&index), sizeof(uint16_t));
			
			sampleChunk.writeType(m_name.data(), EOS_E4_MAX_NAME_LEN);
			
			sampleChunk.writeType(m_midiData.data(), m_midiData.size());
		}
		
		void Read(std::ifstream& stream, const size_t subChunkSize)
		{
			stream.read(reinterpret_cast<char*>(&m_index), sizeof(uint16_t));
			m_index = byteswap_helpers::byteswap_uint16(m_index);

			m_name.clear();
			m_name.resize(EOS_E4_MAX_NAME_LEN);
			stream.read(m_name.data(), EOS_E4_MAX_NAME_LEN);

			constexpr size_t SEQ_INFO_WITHOUT_SIZE(sizeof(uint16_t) + EOS_E4_MAX_NAME_LEN);
			
			m_midiData.clear();
			m_midiData.resize(subChunkSize - SEQ_INFO_WITHOUT_SIZE);
			stream.read(reinterpret_cast<char*>(m_midiData.data()), static_cast<std::streamsize>(m_midiData.size()));
		}
		
		void SetMIDIData(std::vector<char>&& data) { m_midiData = std::move(data); }

		void SetIndex(const uint16_t index)
		{
			// Max indicates that the index will be automatically assigned.
			if(index == std::numeric_limits<uint16_t>::max()) { m_index = index; return; }
			
			m_index = std::clamp(index, 0ui16, static_cast<uint16_t>(EOS_E4_MAX_SEQUENCES));
		}
		
		void SetName(std::string&& name)
		{
			ApplyEOSNamingStandards(name);
			m_name = std::move(name);
		}

		[[nodiscard]] uint16_t GetIndex() const { return m_index; }
		[[nodiscard]] const std::string& GetName() const { return m_name; }
		[[nodiscard]] const std::vector<char>& GetMIDIData() const { return m_midiData; }
		
	private:
		uint16_t m_index = std::numeric_limits<uint16_t>::max(); // requires byteswap
		std::string m_name;
		std::vector<char> m_midiData{};
	};

	/*
	 * Startup
	 */

	struct E4MIDIChannel final
	{
		E4MIDIChannel() = default;
		
		uint8_t m_volume = 127ui8; // [0, 127]
		int8_t m_pan = 0i8; // [-64, 63]
		std::array<uint8_t, 3> m_possibleRedundant1{};
		uint8_t m_aux = 255ui8; // 0 = off, 255 = on
		std::array<uint8_t, 16> m_controllers{}; // [0, 127]
		std::array<uint8_t, 8> m_possibleRedundant2{'\0', '\0', '\0', '\0', 127ui8};
		uint16_t m_presetNum = 65535ui16; // 65535 = none
	};

	struct E4EMSt final
	{
		E4EMSt() = default;
		
		explicit E4EMSt(std::string&& emstName, const uint16_t currentPreset) : m_currentPreset(currentPreset)
		{
			ApplyEOSNamingStandards(emstName);
			m_name = std::move(emstName);
		}

		void Write(FORMChunk& emstChunk) const
		{
			const char* null(nullptr);
			emstChunk.writeType(null, sizeof(uint16_t));
			
			emstChunk.writeType(m_name.data(), EOS_E4_MAX_NAME_LEN);
			
			emstChunk.writeType(null, 4);

			emstChunk.writeType(reinterpret_cast<const char*>(&m_currentPreset), sizeof(uint16_t));
			emstChunk.writeType(reinterpret_cast<const char*>(&m_midiChannels), static_cast<std::streamsize>(sizeof(E4MIDIChannel) * m_midiChannels.size()));

			emstChunk.writeType(null, 5);

			emstChunk.writeType(reinterpret_cast<const char*>(&m_tempo), sizeof(uint8_t));

			emstChunk.writeType(null, 312);
		}
		
		void Read(std::ifstream& stream)
		{
			stream.ignore(2);

			m_name.clear();
			m_name.resize(EOS_E4_MAX_NAME_LEN);
			stream.read(m_name.data(), EOS_E4_MAX_NAME_LEN);

			stream.ignore(4);
			
			stream.read(reinterpret_cast<char*>(&m_currentPreset), sizeof(uint16_t));
			m_currentPreset = byteswap_helpers::byteswap_uint16(m_currentPreset);
			
			stream.read(reinterpret_cast<char*>(&m_midiChannels), static_cast<std::streamsize>(sizeof(E4MIDIChannel) * m_midiChannels.size()));
			
			stream.ignore(5);
			
			stream.read(reinterpret_cast<char*>(&m_tempo), sizeof(uint8_t));
			
			stream.ignore(312);
		}

		void SetCurrentPreset(const uint16_t presetIndex) { m_currentPreset = presetIndex; }
		void SetTempo(const uint8_t tempo) { m_tempo = std::clamp(tempo, 20ui8, 240ui8); }

		void SetName(std::string&& name)
		{
			ApplyEOSNamingStandards(name);
			m_name = std::move(name);
		}

		[[nodiscard]] const std::string& GetName() const { return m_name; }
		[[nodiscard]] uint16_t GetCurrentPreset() const { return m_currentPreset; }
		[[nodiscard]] const std::array<E4MIDIChannel, 32>& GetMIDIChannels() const { return m_midiChannels; }
		[[nodiscard]] uint8_t GetTempo() const { return m_tempo; }

	private:
		std::string m_name;
		uint16_t m_currentPreset = 0ui16;
		std::array<E4MIDIChannel, 32> m_midiChannels{};
		uint8_t m_tempo = 20ui8; // [20, 240]
	};
}