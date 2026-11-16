%global nvver 610.43.03

Name: gpu-unlock
Version: 2.4.0
Release: 1%{?dist}
Summary: GeForce BF16 unlock before the NVIDIA driver loads
License: GPL-2.0-only
BuildArch: noarch
Source0: gpu-unlock-%{version}.tar.xz
Source1: https://download.nvidia.com/XFree86/NVIDIA-kernel-module-source/NVIDIA-kernel-module-source-%{nvver}.tar.xz
Source2: https://download.nvidia.com/XFree86/Linux-x86_64/%{nvver}/NVIDIA-Linux-x86_64-%{nvver}-no-compat32.run
# NVIDIA inputs stay out of the source RPM (it becomes a .nosrc.rpm).
NoSource: 1
NoSource: 2
BuildRequires: make
BuildRequires: gcc
BuildRequires: python3
BuildRequires: zstd

%description
Kernel module that unlocks GeForce BF16 throughput before the unmodified
NVIDIA driver binds, opted in on the kernel command line.

%package dkms
Summary: GeForce BF16 unlock kernel source
Requires: dkms
Requires(post): dkms
Requires(preun): dkms
Requires: gpu-unlock-firmware = %{version}-%{release}

%description dkms
DKMS source for gpu_unlock, loaded ahead of nvidia on every boot. It does
nothing unless gpu_unlock.targets= is on the kernel command line.

%package firmware
Summary: Locally generated pinned NVIDIA firmware for GPU unlock
License: LicenseRef-NVIDIA AND GPL-2.0-only

%description firmware
Firmware assets generated locally from verified upstream inputs.

%prep
sha256sum -c - <<EOF
7e118923c7a23edc36114d63273a46e3e04e9af98695a42203e7ac2dfe9fc1dc  %{SOURCE1}
e084451db730a6df29d3d74f655749c4d236e3c945f7d94b5205e84318fe2f90  %{SOURCE2}
EOF
%autosetup
test "$(cat VERSION)" = "%{version}"

%build
%make_build INPUTS=%{_sourcedir} ASSETS="$PWD/assets" firmware

%install
make DESTDIR=%{buildroot} ASSETS="$PWD/assets" \
    install-dkms install-firmware install-dracut

%files dkms
%license licenses/GPL-2.0.txt licenses/CMP40HX-MIT.txt licenses/NVIDIA-MIT.txt
%doc %{_datadir}/doc/gpu-unlock
%{_usrsrc}/gpu-unlock-%{version}
%{_prefix}/lib/dracut/dracut.conf.d/00-gpu-unlock.conf

%files firmware
%defattr(0644,root,root,0755)
%license %{_defaultlicensedir}/gpu-unlock-firmware
%{_prefix}/lib/firmware/gpu-unlock

%post dkms
# Before 1.0.1 the old package kept its DKMS entry on upgrade; drop any
# other version while its source is still on disk.
for d in /var/lib/dkms/gpu-unlock/[0-9]*; do
    [ -d "$d" ] && [ "${d##*/}" != %{version} ] &&
        dkms remove -m gpu-unlock -v "${d##*/}" --all || :
done
dkms add -m gpu-unlock -v %{version} --rpm_safe_upgrade || :

%preun dkms
# On an upgrade to the same version --rpm_safe_upgrade keeps the entry;
# on any other upgrade the old version must go too.
if [ -d /var/lib/dkms/gpu-unlock/%{version} ]; then
    dkms remove -m gpu-unlock -v %{version} --all --rpm_safe_upgrade || :
fi

%posttrans dkms
# Build for every kernel that has headers, not just the running one: a
# kernel updated but not yet booted would otherwise boot without the
# module. Kernels installed later get it from the dkms kernel-install hook.
kernels=
for b in /lib/modules/*/build; do
    [ -d "$b" ] || continue
    k=${b%/build}; k=${k##*/}
    dkms build -m gpu-unlock -v %{version} -k "$k" --force && kernels="$kernels -k $k"
done
[ -n "$kernels" ] && dkms install -m gpu-unlock -v %{version} $kernels --force || :
# The initramfs has to carry gpu_unlock and its firmware so that it runs
# before nvidia. Fedora's and EPEL's dkms regenerate every image after an
# install (post_transaction in framework.conf); otherwise rebuild those of
# the kernels that can load it.
#
# Pass --force-drivers explicitly: when gpu_unlock is already loaded on the
# running kernel (any in-place upgrade), dracut's hostonly mode picks the
# module up as a loaded module and never runs the force_drivers drop-in, so
# the firmware is left out of the rebuilt image.
if command -v dracut >/dev/null &&
    ! grep -qs '^post_transaction=.*dracut' /etc/dkms/framework.conf \
        /etc/dkms/framework.conf.d/*.conf; then
    for d in /lib/modules/*/; do
        k=${d%/}; k=${k##*/}
        modinfo -k "$k" gpu_unlock >/dev/null 2>&1 &&
            dracut --force --kver "$k" --force-drivers gpu_unlock || :
    done
fi

%postun dkms
if [ "$1" -eq 0 ]; then
    # A kernel package's weak-modules run can still have linked another
    # kernel's build into its weak-updates; such a link now dangles and
    # makes dracut fail for that kernel.
    for l in /lib/modules/*/weak-updates/gpu_unlock.ko*; do
        [ -L "$l" ] || continue
        rm -f "$l"
        k=${l%/weak-updates/*}; depmod -a "${k##*/}" || :
    done
    # Rebuild every initramfs that still loads the removed module. dkms
    # remove may already have regenerated them, but only while the dracut
    # drop-in was still there.
    if command -v lsinitrd >/dev/null; then
        for d in /lib/modules/*/; do
            k=${d%/}; k=${k##*/}
            lsinitrd --kver "$k" 2>/dev/null | grep -q gpu_unlock &&
                dracut --force --kver "$k" || :
        done
    fi
fi

%changelog
* Tue Nov 17 2026 Zagitta <zetlon+github@gmail.com> - 2.4.0-1
- Prepare the Booter and the Falcon loader of every selected card, not of
  every firmware bundle, so a family that shares another family's GSP boot
  gets its own Booter image in a mixed round.
- Derive the TU116 Booter, payload and family from the TU116 bindata of the
  pinned driver, and ship booter_load_tu116.bin. The die gets no card id:
  with its own Booter the transaction completes and the stock driver then
  refuses the card, so it is material only, as GA100 is.
- Make the TrueNAS kernel-headers gate's assertion fail the job; a pipeline
  beginning with ! is exempt from errexit, so it never could.
- Drop the apostrophes from the AUR publish prompts; bash parsed them as
  quotes inside ${VAR:?...} and left the script unparseable.

* Mon Nov 16 2026 Zagitta <zetlon+github@gmail.com> - 2.3.0-1
- Add a packager-free build and install path (make install-local) for
  distros with no native packager.
- Add TrueNAS SCALE, Guix System, rpm-ostree and NixOS support, and
  openSUSE packaging.
- Contain a late unlock failure with the success path's FLR, so a Booter
  that halts after the privileged writes leaves the card re-unlockable at
  the next boot instead of held until power off.
- Accept the NvU64-word bindata codegen of the newer NVIDIA source trees.
- Name the modules to dracut when rebuilding on install.

* Mon Nov 16 2026 Zagitta <zetlon+github@gmail.com> - 2.2.0-1
- Unlock Turing (TU102, TU104 and TU106) with the two-write chain: the
  Booter opens WPR_CFG_PLM and then the FEAT aperture, and the GSP launch
  tolerates a halted engine before the FWSEC v2 loader, settles after the
  GSP reset and polls the TU10x scrub in DMACTL.
- Add the GA103M RTX 3080 Ti Laptop id.
- Document unlocking for virtualized GPUs.
- Run the host unit tests of the parsers and the recipe table in CI, and
  assert all three initramfs-tools drop-ins in the package round-trip.

* Thu Nov 12 2026 Zagitta <zetlon+github@gmail.com> - 2.1.0-1
- Add GA107 and the reported GA106 and AD106 laptop SKUs.
- Pad Booter DMEM sizes to the Falcon DMA granularity.
- Build on kernels older than 5.16, which have no pci_dev_lock().
- Rebuild dracut initramfs images on Debian and Ubuntu too, and load
  gpu_unlock ahead of nvidia by naming the dracut drop-in
  00-gpu-unlock.conf.
- Add tools/bench_gemm.c.

* Tue Oct 27 2026 Zagitta <zetlon+github@gmail.com> - 2.0.0-1
- Add Turing and Ada Lovelace support.

* Mon Sep 28 2026 Zagitta <zetlon+github@gmail.com> - 1.1.0-1
- Initial release.
