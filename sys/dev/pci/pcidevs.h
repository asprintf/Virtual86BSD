/*
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* pcidevs.h - the PCI vendors and devices this kernel knows by name: those
   of QEMU's machines (and the e1000 variants the driver takes).  The
   defines are for the drivers; the tables (PCIDEVS_DATA, once, in pci.c)
   give the names dmesg shows: "Intel 82540EM". */
#ifndef _DEV_PCI_PCIDEVS_H_
#define _DEV_PCI_PCIDEVS_H_

#define PCI_VENDOR_INTEL 0x8086
#define PCI_VENDOR_CIRRUS 0x1013
#define PCI_VENDOR_QEMU 0x1234 /* Bochs and QEMU's display adapter */
#define PCI_VENDOR_QUMRANET 0x1af4 /* virtio */
#define PCI_VENDOR_REDHAT 0x1b36

#define PCI_PRODUCT_INTEL_82441FX 0x1237 /* i440FX host bridge */
#define PCI_PRODUCT_INTEL_82371SB_ISA 0x7000 /* PIIX3 */
#define PCI_PRODUCT_INTEL_82371SB_IDE 0x7010
#define PCI_PRODUCT_INTEL_82371AB_PM 0x7113 /* PIIX4 power management */
#define PCI_PRODUCT_INTEL_82543GC 0x1004 /* e1000 */
#define PCI_PRODUCT_INTEL_82540EM 0x100e
#define PCI_PRODUCT_INTEL_82545EM 0x100f
#define PCI_PRODUCT_INTEL_82544GC 0x100c
#define PCI_PRODUCT_INTEL_82540EM_LOM 0x1015
#define PCI_PRODUCT_INTEL_82547GI 0x1019
#define PCI_PRODUCT_INTEL_82540EP 0x101e
#define PCI_PRODUCT_INTEL_82574L 0x10d3
#define PCI_PRODUCT_INTEL_82801IB_LPC 0x2918 /* ICH9 (q35) */
#define PCI_PRODUCT_INTEL_82P35_HB 0x29c0 /* q35 host bridge */
#define PCI_PRODUCT_INTEL_82801IR_SATA 0x2922
#define PCI_PRODUCT_INTEL_82801I_SMB 0x2930
#define PCI_PRODUCT_CIRRUS_CL_GD5446 0x00b8
#define PCI_PRODUCT_QEMU_VGA 0x1111
#define PCI_PRODUCT_QUMRANET_VIO_NET 0x1000
#define PCI_PRODUCT_QUMRANET_VIO_BLOCK 0x1001
#define PCI_PRODUCT_QUMRANET_VIO_BALLOON 0x1002
#define PCI_PRODUCT_QUMRANET_VIO_CONSOLE 0x1003
#define PCI_PRODUCT_QUMRANET_VIO_SCSI 0x1004
#define PCI_PRODUCT_QUMRANET_VIO_RNG 0x1005
#define PCI_PRODUCT_REDHAT_PCIE_RP 0x000c /* QEMU PCIe root port */

/* class (config register 8, bits 31..8: class, subclass, interface) */
#define PCI_CLASS(x) (((x) >> 24) & 0xff)
#define PCI_SUBCLASS(x) (((x) >> 16) & 0xff)
#define PCI_REVISION(x) ((x) & 0xff)
#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_MASS_STORAGE_IDE 0x01
#define PCI_CLASS_DISPLAY 0x03
#define PCI_SUBCLASS_DISPLAY_VGA 0x00
#define PCI_CLASS_BRIDGE 0x06
#define PCI_SUBCLASS_BRIDGE_HOST 0x00
#define PCI_SUBCLASS_BRIDGE_ISA 0x01

#ifdef PCIDEVS_DATA
struct pci_vendor_name {
  uint16_t vendor;
  const char *name;
};
struct pci_product_name {
  uint16_t vendor, product;
  const char *name;
};

static const struct pci_vendor_name pci_vendors[] = {
    {PCI_VENDOR_INTEL, "Intel"},
    {PCI_VENDOR_CIRRUS, "Cirrus Logic"},
    {PCI_VENDOR_QEMU, "QEMU"},
    {PCI_VENDOR_QUMRANET, "Qumranet"},
    {PCI_VENDOR_REDHAT, "Red Hat"},
    {0, NULL},
};

static const struct pci_product_name pci_products[] = {
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82441FX, "82441FX"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82371SB_ISA, "82371SB ISA"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82371SB_IDE, "82371SB IDE"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82371AB_PM, "82371AB Power"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC, "82543GC"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM, "82540EM"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM, "82545EM"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC, "82544GC"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM_LOM, "82540EM LOM"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547GI, "82547GI"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP, "82540EP"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82574L, "82574L"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801IB_LPC, "82801IB LPC"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82P35_HB, "82P35 Host"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801IR_SATA, "82801IR SATA"},
    {PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801I_SMB, "82801I SMBus"},
    {PCI_VENDOR_CIRRUS, PCI_PRODUCT_CIRRUS_CL_GD5446, "CL-GD5446"},
    {PCI_VENDOR_QEMU, PCI_PRODUCT_QEMU_VGA, "Standard VGA"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_NET, "Virtio Network"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_BLOCK, "Virtio Storage"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_BALLOON, "Virtio Memory Balloon"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_CONSOLE, "Virtio Console"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_SCSI, "Virtio SCSI"},
    {PCI_VENDOR_QUMRANET, PCI_PRODUCT_QUMRANET_VIO_RNG, "Virtio Entropy"},
    {PCI_VENDOR_REDHAT, PCI_PRODUCT_REDHAT_PCIE_RP, "PCIe Root Port"},
    {0, 0, NULL},
};
#endif /* PCIDEVS_DATA */

#endif
