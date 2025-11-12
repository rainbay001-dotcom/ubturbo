Name:          smap_tiering
Version:       %{ver}
Release:       %{rel}
Summary:       Huawei smap tiering driver
License:       GPLv2
URL:           https://support.huawei.com
Provides:      %{name}
Vendor:        Huawei Technologies Co., Ltd.
BuildRoot:     %{buildroot}
ExclusiveArch: %{arch}
Requires:      kernel

%define driverdir /lib/modules/smap_tiering

%description
This package contains the Huawei SMAP tiering Driver

%prep
echo "########no need to prepare the build########"

%build
echo "########no build, already build before########"

%install
echo "########RPM_BUILD_ROOT=${RPM_BUILD_ROOT}"
rm -rf ${RPM_BUILD_ROOT}
mkdir -p -m755 ${RPM_BUILD_ROOT}/%{driverdir}
%{__install} -b -m 0644 %{_builddir}/smap_tiering.ko ${RPM_BUILD_ROOT}/%{driverdir}
%{__install} -b -m 0644 %{_builddir}/start_tiering.sh ${RPM_BUILD_ROOT}/%{driverdir}
%{__install} -b -m 0644 %{_builddir}/stop_tiering.sh ${RPM_BUILD_ROOT}/%{driverdir}

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%{driverdir}/smap_tiering.ko
%{driverdir}/start_tiering.sh
%{driverdir}/stop_tiering.sh

%pre

%post
cd %{driverdir}
depmod -a
echo "external 5.10.0-* /lib/modules/smap_tiering" > /etc/depmod.d/smap_tiering.conf
cat start_tiering.sh > /usr/local/bin/start_tiering
cat stop_tiering.sh > /usr/local/bin/stop_tiering
chmod +x /usr/local/bin/start_tiering
chmod +x /usr/local/bin/stop_tiering
depmod -a

%preun
lsmod | grep smap_tiering
if [ "$?" = "0" ]; then
    modprobe -r smap_tiering
fi

%postun
if [ "$1" = "0" ]; then
	rm -f /etc/depmod.d/smap_tiering.conf
fi
depmod -a

%changelog
