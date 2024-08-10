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


#define INTERP_SHIFT	(14)
#define INTERP_SIZE		(1 << INTERP_SHIFT)


CSC55Synth::CSC55Synth(unsigned nSampleRate)
	: CSynthBase(nSampleRate)
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

	for (int i = 0; i < ROM_SET_COUNT; i++)
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
		SC55_Open(&romimage);
	}

	memset(interp_prev_sample, 0, sizeof(interp_prev_sample));
	memset(interp_cur_sample, 0, sizeof(interp_cur_sample));
	interp_pos = INTERP_SIZE;
	interp_ratio = SC55_SampleFreq() * INTERP_SIZE / m_nSampleRate;

	for (int j = 0; j < ROM_SET_N_FILES; j++)
	{
		if (romimage.image[j].data)
		{
			free(romimage.image[j].data);
		}
		romimage.image[j].size = 0;
	}

	SC55_Update(interp_prev_sample);

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
}

size_t CSC55Synth::Render(s16* pOutBuffer, size_t nFrames)
{
	size_t samples = nFrames;
	s16 *buff = pOutBuffer;

	m_Lock.Acquire();
	while (samples--)
	{
		while (interp_pos >= INTERP_SIZE)
		{
			interp_prev_sample[0] = interp_cur_sample[0];
			interp_prev_sample[1] = interp_cur_sample[1];
			SC55_Update(interp_cur_sample);
			interp_pos -= INTERP_SIZE;
		}
		interp_pos += interp_ratio;
#if 0
		*buff++ = ((int)interp_prev_sample[0] * (INTERP_SIZE - interp_pos)
				   + (int)interp_cur_sample[0] * (interp_pos)) / INTERP_SIZE;
		*buff++ = ((int)interp_prev_sample[1] * (INTERP_SIZE - interp_pos)
				   + (int)interp_cur_sample[1] * (interp_pos)) / INTERP_SIZE;
#else
		*buff++ = interp_cur_sample[0];
		*buff++ = interp_cur_sample[1];
#endif
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
		while (interp_pos >= INTERP_SIZE)
		{
			interp_prev_sample[0] = interp_cur_sample[0];
			interp_prev_sample[1] = interp_cur_sample[1];
			SC55_Update(interp_cur_sample);
			interp_pos -= INTERP_SIZE;
		}
		interp_pos += interp_ratio;
#if 0
		int s;
		s = ((int)interp_prev_sample[0] * (INTERP_SIZE - interp_pos)
				   + (int)interp_cur_sample[0] * (interp_pos)) / INTERP_SIZE;
		*buff++ = (float)s / 32768.0f;
		s = ((int)interp_prev_sample[1] * (INTERP_SIZE - interp_pos)
				   + (int)interp_cur_sample[1] * (interp_pos)) / INTERP_SIZE;
		*buff++ = (float)s / 32768.0f;
#else
		*buff++ = (float)interp_cur_sample[0] / 32768.0f;
		*buff++ = (float)interp_cur_sample[1] / 32768.0f;
#endif
	}
	m_Lock.Release();

	return nFrames;
}

void CSC55Synth::ReportStatus() const
{
}

void CSC55Synth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
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
}
