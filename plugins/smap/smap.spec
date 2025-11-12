%global smap_version    1.0.0
%global release_version 1

Name:          ubturbo-smap
Version:       %{smap_version}
Release:       %{release_version}
Summary:       Huawei SMAP driver
License:       GPLv2
URL:           https://support.huawei.com
Source0:       smap.tar.gz
Provides:      %{name}
Vendor:        Huawei Technologies Co., Ltd.
BuildRoot:     %{buildroot}
ExclusiveArch: %arm64
BuildRequires: kernel-devel >= 5.10.0-136.12.0.86 make >= 4.3 gcc >= 10.3.1
BuildRequires: libboundscheck
Requires:      kernel >= 5.10.0-136.12.0.86

%define debug_package %{nil}
%define smap_dir /lib/modules/smap
%define ucache_dir /lib/modules/ucache
%define smap_libsmap_dir /usr/lib64
%define udev_rules_dir %{_sysconfdir}/udev/rules.d

%description
This package contains the Huawei SMAP Driver

%prep
%setup -q -T -b 0 -c -n smap

%build
cd %{_builddir}/smap/src/drivers && make -j`nproc`
cp %{_builddir}/smap/src/drivers/Module.symvers %{_builddir}/smap/src/tiering/depends
cd %{_builddir}/smap/src/tiering && make -j`nproc`
cd %{_builddir}/smap/src/ucache && make -j`nproc`
cd %{_builddir}/smap && cmake -DCMAKE_BUILD_TYPE=Release %{_builddir}/smap
make -j`nproc` install

%install
echo "########RPM_BUILD_ROOT=${RPM_BUILD_ROOT}"
rm -rf ${RPM_BUILD_ROOT}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{smap_dir}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{ucache_dir}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{smap_libsmap_dir}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{udev_rules_dir}
%{__install} -b -m 0500 %{_builddir}/smap/src/drivers/smap_tracking_core.ko ${RPM_BUILD_ROOT}/%{smap_dir}
%{__install} -b -m 0500 %{_builddir}/smap/src/drivers/smap_access_tracking.ko ${RPM_BUILD_ROOT}/%{smap_dir}
%{__install} -b -m 0500 %{_builddir}/smap/src/drivers/smap_histogram_tracking.ko ${RPM_BUILD_ROOT}/%{smap_dir}
%{__install} -b -m 0500 %{_builddir}/smap/src/tiering/smap_tiering.ko ${RPM_BUILD_ROOT}/%{smap_dir}
%{__install} -b -m 0500 %{_builddir}/smap/src/ucache/ucache.ko ${RPM_BUILD_ROOT}/%{ucache_dir}
%{__install} -b -m 0500 %{_builddir}/smap/output/smap/lib/libsmap.so ${RPM_BUILD_ROOT}/%{smap_libsmap_dir}
%{__install} -b -m 0640 %{_builddir}/smap/99-smap.rules ${RPM_BUILD_ROOT}/%{udev_rules_dir}

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,ubturbo,ubturbo)
%{smap_dir}/smap_tracking_core.ko
%{smap_dir}/smap_access_tracking.ko
%{smap_dir}/smap_histogram_tracking.ko
%{smap_dir}/smap_tiering.ko
%{ucache_dir}/ucache.ko
%{smap_libsmap_dir}/libsmap.so
%{udev_rules_dir}/99-smap.rules

%pre

%post
cd %{smap_dir}
depmod -a
echo "external 6.6.0-* %{smap_dir}" > /etc/depmod.d/smap.conf
echo "external 6.6.0-* %{ucache_dir}" > /etc/depmod.d/ucache.conf
depmod -a

%preun
if [ "$1" = "0" ]; then
    modprobe -r smap_tiering
    modprobe -r smap_histogram_tracking
    modprobe -r smap_access_tracking
    modprobe -r smap_tracking_core
    modprobe -r ucache
fi

%postun
if [ "$1" = "0" ]; then
    rm -rf %{smap_dir}
    rm -rf %{ucache_dir}
    rm -f /etc/depmod.d/smap.conf
    rm -f /etc/depmod.d/ucache.conf
    rm -f %{udev_rules_dir}/99-smap.rules
fi
depmod -a

%changelog
