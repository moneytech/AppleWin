#pragma once

#define TMS5220_OUTPUTBUFFER_SIZE 200

class CTMS5220
{
public:
	CTMS5220()
	{
		tms5220_outputbuffer_max = TMS5220_OUTPUTBUFFER_SIZE;

		/* Speak External mode: 1=on 0=off */
		tms5220_speakext = 0;

		//

		tms5220_bits = 0;
		tms5220_buffer = 0;
	}
	virtual ~CTMS5220() {}

	BYTE GetStatus(void) { return m_Status.Data; }

	void tms5220_request(void);
	//void tms5220_request_end(void);
	void tms5220_write(int b);
	void tms5220_reset(void);
	void tms5220_init(void);

private:
	void tms5220_outframe(void);
	void tms5220_bit(int b);
	void tms5220_speakexternal(int d);

	//

public:
	short tms5220_outputbuffer[TMS5220_OUTPUTBUFFER_SIZE];

	int tms5220_outputbuffer_ptr;
	int tms5220_outputbuffer_max;
	int tms5220_mixingrate;

private:
	enum {ACTIVE=1, INACTIVE=0};

	union
	{
		BYTE		Data;
		struct
		{
			BYTE	Reserved : 5;
			BYTE	bBufferEmpty : 1;	// b5
			BYTE	bBufferLow : 1;		// b6
			BYTE	bTalkStatus : 1;	// b7
		};
	} m_Status;

	//

	/* Speak External mode: 1=on 0=off */
	int tms5220_speakext;

	/* Speech parameter FIFO buffer */
	short tms5220_fifo[6000];
	int tms5220_fifohead;
	int tms5220_fifotail;

	/* Parameters used for interpolation */
	short tms5220_param[12];

	/* Excitation pulse counter for voiced segments */
	USHORT tms5220_excite;

	/* Random array for unvoiced segments */
	short tms5220_ranlst[4096];
	int tms5220_ranptr;

	//

	/* Used for decoding input */
	int tms5220_tmp_energy;
	int tms5220_tmp_repeat;
	int tms5220_tmp_pitch;
	int tms5220_tmp_k1;
	int tms5220_tmp_k2;
	int tms5220_tmp_k3;
	int tms5220_tmp_k4;
	int tms5220_tmp_k5;
	int tms5220_tmp_k6;
	int tms5220_tmp_k7;
	int tms5220_tmp_k8;
	int tms5220_tmp_k9;
	int tms5220_tmp_k10;

	/* Lattice filter delay line */
	int tms5220_u[11];
	int tms5220_x[11];

	//

	int tms5220_bits;
	int tms5220_buffer;
	int nrg,rep,pitch,k1,k2,k3,k4,k5,k6,k7,k8,k9,k10;

	//

	static short tms5220_energytable[0x10];
	static USHORT tms5220_pitchtable [0x40];
	static USHORT tms5220_k1table    [0x20];
	static USHORT tms5220_k2table    [0x20];
	static USHORT tms5220_k3table    [0x10];
	static USHORT tms5220_k4table    [0x10];
	static USHORT tms5220_k5table    [0x10];
	static USHORT tms5220_k6table    [0x10];
	static USHORT tms5220_k7table    [0x10];
	static USHORT tms5220_k8table    [0x08];
	static USHORT tms5220_k9table    [0x08];
	static USHORT tms5220_k10table   [0x08];
};
