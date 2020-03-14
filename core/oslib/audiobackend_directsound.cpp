#include "audiostream.h"
#ifdef _WIN32
#include "oslib.h"
#include <initguid.h>
#include <dsound.h>
#ifdef USE_SDL
#include "sdl/sdl.h"
#endif

#define verifyc(x) verify(!FAILED(x))

void* SoundThread(void* param);
#define V2_BUFFERSZ (16*1024)

static IDirectSound8* dsound;
static IDirectSoundBuffer8* buffer;

static u32 ds_ring_size;

static void directsound_init()
{
	verifyc(DirectSoundCreate8(NULL,&dsound,NULL));

#ifdef USE_SDL
	verifyc(dsound->SetCooperativeLevel(sdl_get_native_hwnd(), DSSCL_PRIORITY));
#else
	verifyc(dsound->SetCooperativeLevel((HWND)libPvr_GetRenderTarget(), DSSCL_PRIORITY));
#endif
	IDirectSoundBuffer* buffer_;

	WAVEFORMATEX wfx;
	DSBUFFERDESC desc;

	// Set up WAV format structure.

	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.nBlockAlign = 4;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.wBitsPerSample = 16;

	// Set up DSBUFFERDESC structure.

	ds_ring_size=8192*wfx.nBlockAlign;

	memset(&desc, 0, sizeof(DSBUFFERDESC));
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;// _CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;

	desc.dwBufferBytes = ds_ring_size;
	desc.lpwfxFormat = &wfx;



	if (settings.aica.HW_mixing==0)
	{
		desc.dwFlags |=DSBCAPS_LOCSOFTWARE;
	}
	else if (settings.aica.HW_mixing==1)
	{
		desc.dwFlags |=DSBCAPS_LOCHARDWARE;
	}
	else if (settings.aica.HW_mixing==2)
	{
		//auto
	}
	else
	{
		die("settings.HW_mixing: Invalid value");
	}

	if (settings.aica.GlobalFocus)
		desc.dwFlags|=DSBCAPS_GLOBALFOCUS;

	verifyc(dsound->CreateSoundBuffer(&desc,&buffer_,0));
	verifyc(buffer_->QueryInterface(IID_IDirectSoundBuffer8,(void**)&buffer));
	buffer_->Release();

	//Play the buffer !
	verifyc(buffer->Play(0,0,DSBPLAY_LOOPING));

}


static DWORD wc=0;


static int directsound_getfreesz()
{
	DWORD pc,wch;

	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;
	return fsz;
}

static int directsound_getusedSamples()
{
	return (ds_ring_size-directsound_getfreesz())/4;
}

static u32 directsound_push_nw(const void* frame, u32 samplesb)
{
	DWORD pc,wch;

	u32 bytes=samplesb*4;

	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;

	//printf("%d: r:%d w:%d (f:%d wh:%d)\n",fsz>bytes,pc,wc,fsz,wch);

	if (fsz>bytes)
	{
		void* ptr1,* ptr2;
		DWORD ptr1sz,ptr2sz;

		const u8* data=(const u8*)frame;

		buffer->Lock(wc,bytes,&ptr1,&ptr1sz,&ptr2,&ptr2sz,0);
		memcpy(ptr1,data,ptr1sz);
		if (ptr2sz)
		{
			data+=ptr1sz;
			memcpy(ptr2,data,ptr2sz);
		}

		buffer->Unlock(ptr1,ptr1sz,ptr2,ptr2sz);
		wc=(wc+bytes)%ds_ring_size;
		return 1;
	}
	return 0;
	//ds_ring_size
}

static u32 directsound_push(const void* frame, u32 samples, bool wait)
{
	while (!directsound_push_nw(frame, samples) && wait)
		//DEBUG_LOG(AUDIO, "FAILED waiting on audio FAILED %d", directsound_getusedSamples())
		;

	return 1;
}

static void directsound_term()
{
	buffer->Stop();

	buffer->Release();
	dsound->Release();
}

static audiobackend_t audiobackend_directsound = {
    "directsound", // Slug
    "Microsoft DirectSound", // Name
    &directsound_init,
    &directsound_push,
    &directsound_term,
	NULL
};

static bool ds = RegisterAudioBackend(&audiobackend_directsound);
#endif
