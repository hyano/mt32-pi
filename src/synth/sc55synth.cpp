//
// sc55synth.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#include <circle/logger.h>
#include <circle/timer.h>
#include <fatfs/ff.h>
#include <stdlib.h>

#include "config.h"
#include "lcd/ui.h"
#include "synth/sc55synth.h"
#include "utility.h"

LOGMODULE("sc55synth");
const char* const Disks[] = { "SD", "USB" };
const char ROMDirectory[] = "sc55_roms";

static const int ROM_SET_N_FILES = 6;

const char* rs_name[ROM_SET_COUNT] = {
    "SC-55mk2",
    "SC-55st",
    "SC-55mk1",
    "CM-300/SCC-1",
    "JV-880",
    "SCB-55",
    "RLP-3237",
    "SC-155",
    "SC-155mk2"
};

static const char* roms[ROM_SET_COUNT][ROM_SET_N_FILES] =
{
    "rom1.bin",
    "rom2.bin",
    "waverom1.bin",
    "waverom2.bin",
    "rom_sm.bin",
    "",

    "rom1.bin",
    "rom2_st.bin",
    "waverom1.bin",
    "waverom2.bin",
    "rom_sm.bin",
    "",

    "sc55_rom1.bin",
    "sc55_rom2.bin",
    "sc55_waverom1.bin",
    "sc55_waverom2.bin",
    "sc55_waverom3.bin",
    "",

    "cm300_rom1.bin",
    "cm300_rom2.bin",
    "cm300_waverom1.bin",
    "cm300_waverom2.bin",
    "cm300_waverom3.bin",
    "",

    "jv880_rom1.bin",
    "jv880_rom2.bin",
    "jv880_waverom1.bin",
    "jv880_waverom2.bin",
    "jv880_waverom_expansion.bin",
    "jv880_waverom_pcmcard.bin",

    "scb55_rom1.bin",
    "scb55_rom2.bin",
    "scb55_waverom1.bin",
    "scb55_waverom2.bin",
    "",
    "",

    "rlp3237_rom1.bin",
    "rlp3237_rom2.bin",
    "rlp3237_waverom1.bin",
    "",
    "",
    "",

    "sc155_rom1.bin",
    "sc155_rom2.bin",
    "sc155_waverom1.bin",
    "sc155_waverom2.bin",
    "sc155_waverom3.bin",
    "",

    "rom1.bin",
    "rom2.bin",
    "waverom1.bin",
    "waverom2.bin",
    "rom_sm.bin",
    "",
};


CSC55Synth::CSC55Synth(unsigned nSampleRate)
	: CSynthBase(nSampleRate), m_volume(100)
{
}

CSC55Synth::~CSC55Synth()
{
}

static bool open_sc55_rom(FIL *fp, const TCHAR *path)
{
	CString DirectoryPath;
	FRESULT result;

	for (auto disk: Disks)
	{
		DirectoryPath.Format("%s:/%s/", disk, ROMDirectory);
		CString fname(static_cast<const char*>(DirectoryPath));
		fname.Append(path);
		result = f_open(fp, fname, FA_READ);
		if (result == FR_OK)
		{
			return true;
		}
	}

	return false;
}

bool CSC55Synth::Initialize()
{
	SC55_RomImage_t romimage;
	FIL fp;
	FRESULT result;
	bool good = false;

	memset(&romimage, 0, sizeof(SC55_RomImage_t));

	//for (int i = 0; i < ROM_SET_COUNT; i++)
	for (int i = ROM_SET_MK1; i < ROM_SET_MK1 + 1; i++)
	{
		good = true;
		for (int j = 0; j < ROM_SET_N_FILES; j++)
		{
			free(romimage.image[j].data);
			romimage.image[j].size = 0;
		}
		for (int j = 0; j < ROM_SET_N_FILES; j++)
		{
			if (strcmp(roms[i][j], "") == 0)
			{
				continue;
			}
			if (!open_sc55_rom(&fp, roms[i][j]))
			{
				good = false;
				break;
			}

			romimage.image[j].size = f_size(&fp);
			romimage.image[j].data = (uint8_t *)malloc(romimage.image[j].size);

			UINT nRead;
			result = f_read(&fp, romimage.image[j].data, romimage.image[j].size, &nRead);

			f_close(&fp);

			if (result != FR_OK)
			{
				good = false;
				break;
			}
		}
		if (good)
		{
			romimage.romset = i;
			romset = i;
			break;
		}
	}

	if (good)
	{
		LOGDBG("ROM Set: %d: %s", romimage.romset, rs_name[romimage.romset]);
		for (int i = 0; i < ROM_SET_N_FILES; i++)
		{
			if (romimage.image[i].data != NULL)
			{
				LOGDBG("  ROM[%d]: %08x: %s", i, romimage.image[i].size, roms[romimage.romset][i]);
			}
		}

		SC55_Open(&romimage);
	}

	memset(m_sample, 0, sizeof(m_sample));
	m_step = (int32_t)(int64_t(m_wav_step) * m_nSampleRate / (SC55_SampleFreq()));
	m_pos = 0;
	m_wav_pos = 0;
	LOGDBG("SC55    Freq: %d", SC55_SampleFreq());
	LOGDBG("mt32-pi Freq: %d", m_nSampleRate);
	LOGDBG("m_wav_step  : %d", m_wav_step);
	LOGDBG("m_step      : %d", m_step);


	memset(lcd_buffer_prev, 0, sizeof(lcd_buffer_prev));

	for (int j = 0; j < ROM_SET_N_FILES; j++)
	{
		if (romimage.image[j].data)
		{
			free(romimage.image[j].data);
		}
		romimage.image[j].size = 0;
	}

	SC55_Update(m_sample);

	if (!good)
	{
		return false;
	}

	return true;
}

void CSC55Synth::HandleMIDIShortMessage(u32 nMessage)
{
	u32 msg = nMessage;

	last_msg = msg;

	//m_Lock.Acquire();
	SC55_Write(msg & 0xff);
	switch (msg & 0xf0)
	{
		case 0xc0:
		case 0xd0:
			msg >>= 8;
			SC55_Write(msg & 0xff);
			break;
		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		case 0xe0:
			msg >>= 8;
			SC55_Write(msg & 0xff);
			msg >>= 8;
			SC55_Write(msg & 0xff);
			break;
		case 0xf0:
			break;
	}
	//m_Lock.Release();

	// Update MIDI monitor
	CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CSC55Synth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	//m_Lock.Acquire();
	for (size_t i = 0; i < nSize; i++)
	{
		SC55_Write(*pData++);
	}
	//m_Lock.Release();
}

bool CSC55Synth::IsActive()
{
	return true;
}

void CSC55Synth::AllSoundOff()
{
	// Reset MIDI monitor
	CSynthBase::AllSoundOff();
}

void CSC55Synth::SetMasterVolume(u8 nVolume)
{
	m_volume = nVolume;
}

size_t CSC55Synth::Render(s16* pOutBuffer, size_t nFrames)
{
	size_t samples = nFrames;
	s16 *buff = pOutBuffer;

	m_Lock.Acquire();
	while (samples--)
	{
		int32_t period;
		int32_t diff;
		int32_t out[2];

		period = m_pos - m_wav_pos;
		period = period < m_wav_step ? period : m_wav_step;
		out[0] = m_sample[0] * period;
		out[1] = m_sample[1] * period;

		m_wav_pos += m_wav_step;
		diff = m_wav_pos - m_pos;

		while (diff > 0)
		{
			SC55_Update(m_sample);

			period = diff < m_step ? diff : m_step;
			out[0] = m_sample[0] * period;
			out[1] = m_sample[1] * period;

			m_pos += m_step;
			diff -= m_step;
		}

		*buff++ = out[0] / m_wav_step * m_volume / 100;
		*buff++ = out[1] / m_wav_step * m_volume / 100;
	}
	m_Lock.Release();

	return nFrames;
}

size_t CSC55Synth::Render(float* pOutBuffer, size_t nFrames)
{
	size_t samples = nFrames;
	float *buff = pOutBuffer;

	m_Lock.Acquire();
	while (samples--)
	{
		int32_t period;
		int32_t diff;
		int32_t out[2];

		period = m_pos - m_wav_pos;
		period = period < m_wav_step ? period : m_wav_step;
		out[0] = m_sample[0] * period;
		out[1] = m_sample[1] * period;

		m_wav_pos += m_wav_step;
		diff = m_wav_pos - m_pos;

		while (diff > 0)
		{
			SC55_Update(m_sample);

			period = diff < m_step ? diff : m_step;
			out[0] = m_sample[0] * period;
			out[1] = m_sample[1] * period;

			m_pos += m_step;
			diff -= m_step;
		}

		*buff++ = (float)out[0] / m_wav_step / 32768.f * m_volume / 100.f;
		*buff++ = (float)out[1] / m_wav_step / 32768.f * m_volume / 100.f;
	}
	m_Lock.Release();

	return nFrames;
}

void CSC55Synth::ReportStatus() const
{
}

void CSC55Synth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
#if 0
	const u8 nHeight = LCD.Height();
	float ChannelLevels[16], PeakLevels[16];
	u8 nStatusRow, nBarHeight;

	if (LCD.GetType() == CLCD::TType::Character)
	{
		nStatusRow = nHeight - 1;
		nBarHeight = nHeight - 1;
	}
	else
	{
		nStatusRow = nHeight / 16 - 1;
		nBarHeight = nHeight - 16;
	}

	for (int i = 0; i < 16; i++)
	{
		ChannelLevels[i] = 0.5f;
		PeakLevels[i] = 0.3f;
	}
	CUserInterface::DrawChannelLevels(LCD, nBarHeight, ChannelLevels, PeakLevels, 16, true);

	CString s;
	s.Format("%d:%08x", romset, last_msg);
	LCD.Print(s, 0, nStatusRow, true, false);
#endif

	SC55_LCD_Update();

#if 1
	uint8_t *lcd_buffer = SC55_LCD_Buffer();

	for (int y = 0; y < 64; y++)
	{
		for (int x = 0; x < 128; x++)
		{
			int32_t idx = y * 128 + x;
			uint8_t p = lcd_buffer[idx];
			if (true || lcd_buffer_prev[idx] != p)
			{
				if (p)
				{
					LCD.SetPixel(x, y);
				}
				else
				{
					LCD.ClearPixel(x, y);
				}
				lcd_buffer_prev[idx] = p;
			}
		}
	}
#endif

}
