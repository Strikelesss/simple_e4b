#pragma once
#include <filesystem>
#include "e4b_types.hpp"
#include <iostream>

namespace simple_e4b
{
	struct E4BBank final
	{
		E4BBank() = default;

		void AddPreset(E4Preset&& preset)
		{
			assert(m_presets.size() < EOS_E4_MAX_PRESETS);
			if(m_presets.size() >= EOS_E4_MAX_PRESETS) { return; }
			
			for(const auto& existingPreset : m_presets)
			{
				if(existingPreset->GetIndex() == preset.GetIndex())
				{
					assert(existingPreset->GetIndex() != preset.GetIndex());
					return;
				}
			}

			if(preset.GetIndex() == std::numeric_limits<uint16_t>::max())
			{
				preset.SetIndex(static_cast<uint16_t>(m_presets.size()));	
			}

			m_presets.emplace_back(std::make_shared<E4Preset>(std::move(preset)));
		}

		void RemovePreset(const uint16_t presetIndex)
		{
			RemoveIfValid(m_presets, presetIndex);
		}

		void AddSequence(E4Sequence&& sequence)
		{
			assert(m_sequences.size() < EOS_E4_MAX_SEQUENCES);
			if(m_sequences.size() >= EOS_E4_MAX_SEQUENCES) { return; }
			
			for(const auto& existingSequence : m_sequences)
			{
				if(existingSequence->GetIndex() == sequence.GetIndex())
				{
					assert(existingSequence->GetIndex() != sequence.GetIndex());
					return;
				}
			}

			if(sequence.GetIndex() == std::numeric_limits<uint16_t>::max())
			{
				sequence.SetIndex(static_cast<uint16_t>(m_sequences.size()));	
			}

			m_sequences.emplace_back(std::make_shared<E4Sequence>(std::move(sequence)));
		}

		void RemoveSequence(const uint16_t sequenceIndex)
		{
			RemoveIfValid(m_sequences, sequenceIndex);
		}
		
		void AddSample(E3Sample&& sample)
		{
			assert(m_samples.size() < EOS_E4_MAX_SAMPLES);
			if(m_samples.size() >= EOS_E4_MAX_SAMPLES) { return; }
			
			for(const auto& existingSample : m_samples)
			{
				if(existingSample->GetIndex() == sample.GetIndex())
				{
					assert(existingSample->GetIndex() != sample.GetIndex());
					return;
				}
			}

			if(sample.GetIndex() == std::numeric_limits<uint16_t>::max())
			{
				sample.SetIndex(static_cast<uint16_t>(m_samples.size()));	
			}

			m_samples.emplace_back(std::make_shared<E3Sample>(std::move(sample)));
		}

		void RemoveSample(const uint16_t sampleIndex)
		{
			RemoveIfValid(m_samples, sampleIndex);
		}
		
		void SetStartupPreset(const uint16_t presetIndex)
		{
			assert(!m_presets.empty());
			if(m_presets.empty()) { return; }
			
			// Startup preset is set to 'None', which is valid behavior.
			if(presetIndex == std::numeric_limits<uint16_t>::max())
			{
				m_startupPreset = presetIndex;
				return;
			}
			
			const auto preset(GetPreset(presetIndex));
			if(!preset.expired())
			{
				m_startupPreset = presetIndex;
			}
			else
			{
				// Otherwise, set to the first valid index:
				m_startupPreset = m_presets[0]->GetIndex();
			}
		}

		[[nodiscard]] std::weak_ptr<E4Preset> GetPreset(const uint16_t presetIndex) const
		{
			const auto& findResult(std::find_if(m_presets.begin(), m_presets.end(), [&](const auto& elem)
			{
				return elem->GetIndex() == presetIndex;
			}));

			if(findResult != m_presets.end()) { return std::weak_ptr<E4Preset>(*findResult); }
			return std::weak_ptr<E4Preset>();
		}

		[[nodiscard]] std::weak_ptr<E3Sample> GetSample(const uint16_t sampleIndex) const
		{
			const auto& findResult(std::find_if(m_samples.begin(), m_samples.end(), [&](const auto& elem)
			{
				return elem->GetIndex() == sampleIndex;
			}));

			if(findResult != m_samples.end()) { return std::weak_ptr<E3Sample>(*findResult); }
			return std::weak_ptr<E3Sample>();
		}

		[[nodiscard]] std::weak_ptr<E4Sequence> GetSequence(const uint16_t sequenceIndex) const
		{
			const auto& findResult(std::find_if(m_sequences.begin(), m_sequences.end(), [&](const auto& elem)
			{
				return elem->GetIndex() == sequenceIndex;
			}));

			if(findResult != m_sequences.end()) { return std::weak_ptr<E4Sequence>(*findResult); }
			return std::weak_ptr<E4Sequence>();
		}

		[[nodiscard]] const std::vector<std::shared_ptr<E4Preset> >& GetPresets() const { return m_presets; }
		[[nodiscard]] const std::vector<std::shared_ptr<E3Sample> >& GetSamples() const { return m_samples; }
		[[nodiscard]] const std::vector<std::shared_ptr<E4Sequence> >& GetSequences() const { return m_sequences; }
		[[nodiscard]] uint16_t GetStartupPreset() const { return m_startupPreset; }

	private:
		template<typename T>
		void RemoveIfValid(std::vector<T>& vector, const uint16_t index)
		{
			bool isValidIndex(false);
			for(const auto& element : vector)
			{
				if(element->GetIndex() == index)
				{
					isValidIndex = true;
					break;
				}
			}

			assert(isValidIndex);
			if(isValidIndex)
			{
				vector.erase(std::next(vector.begin(), static_cast<ptrdiff_t>(index)));
			}
		}
		
		std::vector<std::shared_ptr<E4Preset> > m_presets{};
		std::vector<std::shared_ptr<E3Sample> > m_samples{};
		std::vector<std::shared_ptr<E4Sequence> > m_sequences{};
		uint16_t m_startupPreset = 0ui16;
	};

	enum struct EE4BReadResult final
	{
		READ_SUCCESS, FILE_NOT_EXIST, FILE_INVALID
	};
	
	inline EE4BReadResult ReadE4B(const std::filesystem::path& e4bFile, E4BBank& outBank)
	{
		const bool isEOSFileFormat(e4bFile.extension() == ".e4b" || e4bFile.extension() == ".E4B");
		assert(isEOSFileFormat);
		if(!isEOSFileFormat) { return EE4BReadResult::FILE_INVALID; }
		
		if (std::filesystem::exists(e4bFile))
		{
			std::ifstream stream(e4bFile.c_str(), std::ios::binary);
			if (stream.is_open())
			{
				FORMChunk form;
				form.Read(stream);

				if (form.GetName() == "FORM")
				{
					std::array<char, 4> E4B0{};
					stream.read(E4B0.data(), static_cast<std::streamsize>(E4B0.size()));

					if (std::string_view{E4B0.data(), E4B0.size()} == "E4B0")
					{
						FORMChunk TOC;
						TOC.Read(stream);
						
						if (TOC.GetName() == "TOC1")
						{
							const uint32_t numSubchunks(TOC.GetReadSize() / EOS_E4_TOC_SIZE);
							assert(numSubchunks > 0u);

							if(numSubchunks > 0u)
							{
								for(uint32_t i(0u); i < numSubchunks; ++i)
								{
									// Cache the last position:
									const int64_t cachedStreamPos(stream.tellg());
									
									FORMChunk subChunk;
									subChunk.Read(stream);

									uint32_t subchunkPos(0u);
									stream.read(reinterpret_cast<char*>(&subchunkPos), sizeof(uint32_t));
									subchunkPos = byteswap_helpers::byteswap_uint32(subchunkPos);

									// Skip to the actual data location:
									stream.seekg(subchunkPos + static_cast<uint32_t>(FORM_CHUNK_MAX_NAME_LEN + sizeof(uint32_t)));

									if (subChunk.GetName() == "E4P1")
									{
										E4Preset preset;
										preset.Read(stream);

										outBank.AddPreset(std::move(preset));
									}
									else if (subChunk.GetName() == "E3S1")
									{
										E3Sample sample;
										sample.Read(stream, subChunk.GetReadSize() + 2u);

										outBank.AddSample(std::move(sample));
									}
									else if(subChunk.GetName() == "E4s1")
									{
										E4Sequence sequence;
										sequence.Read(stream, subChunk.GetReadSize() + 2u);

										outBank.AddSequence(std::move(sequence));
									}
									else if (subChunk.GetName() == "E4Ma" || subChunk.GetName() == "EMS0")
									{
										// TODO: E4Ma & Multisetup
										std::cout << "Skipping " << subChunk.GetName() << "!\n";
										stream.ignore(subChunk.GetReadSize());
									}
									else
									{
										return EE4BReadResult::FILE_INVALID;
									}

									// Go to the next location, if applicable:
									if(i + 1u < numSubchunks)
									{
										stream.seekg(cachedStreamPos + static_cast<std::streampos>(EOS_E4_TOC_SIZE));	
									}
									else
									{
										if(!stream.eof())
										{
											subChunk = FORMChunk();
											subChunk.Read(stream);

											if (subChunk.GetName() == "EMSt")
											{
												E4EMSt startup;
												startup.Read(stream);
												
												outBank.SetStartupPreset(startup.GetCurrentPreset());
											}
										}
									}
								}

								return EE4BReadResult::READ_SUCCESS;
							}
						}	
					}

					return EE4BReadResult::FILE_INVALID;
				}
			}
		}
		
		return EE4BReadResult::FILE_NOT_EXIST;
	}

	inline void WriteE4B(const std::filesystem::path& e4bFile, const E4BBank& inBank)
	{
		const bool isEOSFileFormat(e4bFile.extension() == ".e4b" || e4bFile.extension() == ".E4B");
		assert(isEOSFileFormat);
		if(!isEOSFileFormat) { return; }
		
		std::ofstream stream(e4bFile.c_str(), std::ios::binary);
		if (stream.is_open())
		{
			FORMChunk FORM("FORM");

			constexpr std::string_view E4B0("E4B0");
			FORM.writeType(E4B0.data(), E4B0.length());

			FORM.m_subChunks.emplace_back("TOC1");

			const auto AddTOCIndex([&](const FORMChunk& subChunk, uint16_t index, const std::string& name)
			{
				FORMChunk TOCSubchunk(std::string(subChunk.GetName()), subChunk.GetFullSize(false) - 2u);

				// Get the offset including this TOC subchunk:
				const uint32_t chunkLoc(byteswap_helpers::byteswap_uint32(FORM.GetFullSize(true) + EOS_E4_TOC_SIZE));
				TOCSubchunk.writeType(&chunkLoc, sizeof(uint32_t));

				index = byteswap_helpers::byteswap_uint16(index);
				TOCSubchunk.writeType(&index, sizeof(uint16_t));

				TOCSubchunk.writeType(name.c_str(), name.length());

				const char* null(nullptr);
				TOCSubchunk.writeType(null, sizeof(uint16_t));

				FORMChunk& TOC(FORM.m_subChunks[0]);
				TOC.m_subChunks.emplace_back(std::move(TOCSubchunk));
			});
			
			for(const auto& preset : inBank.GetPresets())
			{
				FORMChunk E4P1("E4P1");
				preset->Write(E4P1);

				AddTOCIndex(E4P1, preset->GetIndex(), preset->GetName());
				
				FORM.m_subChunks.emplace_back(std::move(E4P1));
			}
			
			for(const auto& sample : inBank.GetSamples())
			{
				FORMChunk E3S1("E3S1");
				sample->Write(E3S1);

				AddTOCIndex(E3S1, sample->GetIndex(), sample->GetName());
				
				FORM.m_subChunks.emplace_back(std::move(E3S1));
			}

			FORMChunk EMSt("EMSt");
			
			E4EMSt startup("Untitled MSetup ", inBank.GetStartupPreset());
			startup.Write(EMSt);
			
			FORM.m_subChunks.emplace_back(std::move(EMSt));
			
			FORM.Write(stream);
		}
	}
}