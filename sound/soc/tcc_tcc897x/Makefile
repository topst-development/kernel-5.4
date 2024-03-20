# TCC Platform Support
#ifdef CONFIG_SND_SOC_TCC_PCM_V10
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v10.o tcc/tca_tcchwcontrol.o
#else #for TCC898x, TCC802x
#ifdef CONFIG_ARCH_TCC898X
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v20.o tcc/tca_tcchwcontrol.o
#else #for TCC802x
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v20.o tcc/tca_tcchwcontrol.o
#endif
#endif

ifdef CONFIG_ARCH_TCC897X
snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-tcc897x.o
endif

snd-soc-tcc-i2s-objs := tcc-i2s.o
snd-soc-tcc-spdif-objs := tcc-spdif.o
snd-soc-tcc-cdif-objs := tcc-cdif.o

obj-$(CONFIG_SND_SOC_TCC) += snd-soc-tcc-pcm.o snd-soc-tcc-i2s.o snd-soc-tcc-spdif.o snd-soc-tcc-cdif.o

snd-soc-tcc8971-board-lcn30-objs := tcc8971_board_lcn_30.o

obj-$(CONFIG_SND_SOC_TCC8971_BOARD_LCN3) += snd-soc-tcc8971-board-lcn30.o

