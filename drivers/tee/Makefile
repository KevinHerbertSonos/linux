obj-$(CONFIG_TEE) += optee.o
obj-$(CONFIG_OPTEE) += optee/

optee-objs := tee_core.o \
	      tee_shm.o \
	      tee_shm_pool.o \
	      tee_data_pipe.o

ccflags-y += -I$(srctree)/$(src)/include/linux
ccflags-y += -I$(srctree)/$(src)/include
