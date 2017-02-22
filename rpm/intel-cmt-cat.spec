# Copyright (c) 2016, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%global ver014      0.1.4
%global ver015      0.1.5
%global ver0151     0.1.5
%global githubname  intel-cmt-cat
%global githubver   %{ver0151}

%if "%{githubver}" == "%{ver0151}"
%global githubfull  %{githubname}-%{githubver}-1
%else
%global githubfull  %{githubname}-%{githubver}
%endif

%global pkgname     intel-cmt-cat
%if 0%{!?_licensedir}
%global _licensedir %{_docdir}
%endif

Summary:            Provides command line interface to CMT, MBM, CAT and CDP technologies
Name:               %{pkgname}
Version:            %{githubver}

%if "%{githubver}" == "%{ver015}"
Release:            1%{?dist}
%endif
%if "%{githubver}" == "%{ver014}"
Release:            3%{?dist}
%endif

License:            BSD
Group:              Development/Tools
ExclusiveArch:      x86_64 i686 i586
%if "%{githubver}" == "%{ver0151}"
Source0:            https://github.com/01org/%{githubname}/archive/v%{githubver}-1.tar.gz
%else
Source0:            https://github.com/01org/%{githubname}/archive/v%{githubver}.tar.gz
%endif
URL:                https://github.com/01org/%{githubname}

%description
This software package provides basic support for
Cache Monitoring Technology (CMT), Memory Bandwidth Monitoring (MBM),
Cache Allocation Technology (CAT) and Code Data Prioratization (CDP).

CMT, MBM and CAT are configured using Model Specific Registers (MSRs)
to measure last level cache occupancy, set up the class of service masks and
manage the association of the cores/logical threads to a class of service.
The software executes in user space, and access to the MSRs is
obtained through a standard Linux* interface. The virtual file system
provides an interface to read and write the MSR registers but
it requires root privileges.

%prep

%autosetup -n %{githubfull}

%if "%{githubver}" == "%{ver015}"
%post -p /sbin/ldconfig
%endif

%build
make %{?_smp_mflags}

%install
# Not doing make install as it strips the symbols.
# Using files from the build directory.
%if "%{githubver}" == "%{ver015}" || "%{githubver}" == "%{ver0151}"
install -d %{buildroot}/%{_bindir}
install %{_builddir}/%{githubfull}/pqos/pqos %{buildroot}/%{_bindir}

install -d %{buildroot}/%{_mandir}/man8
install -m 0644 %{_builddir}/%{githubfull}/pqos/pqos.8  %{buildroot}/%{_mandir}/man8

install -d %{buildroot}/%{_bindir}
install %{_builddir}/%{githubfull}/rdtset/rdtset %{buildroot}/%{_bindir}

install -d %{buildroot}/%{_mandir}/man8
install -m 0644 %{_builddir}/%{githubfull}/rdtset/rdtset.8  %{buildroot}/%{_mandir}/man8

install -d %{buildroot}/%{_licensedir}/%{name}-%{version}
install -m 0644 %{_builddir}/%{githubfull}/LICENSE %{buildroot}/%{_licensedir}/%{name}-%{version}

install -d %{buildroot}/%{_libdir}
%if "%{githubver}" == "%{ver0151}"
install %{_builddir}/%{githubfull}/lib/libpqos-0.1.6.so %{buildroot}/%{_libdir}
%else
install %{_builddir}/%{githubfull}/lib/libpqos-0.1.5.so %{buildroot}/%{_libdir}
%endif
%endif

%if "%{githubver}" == "%{ver014}"
install -d %{buildroot}/%{_bindir}
install %{_builddir}/%{githubfull}/pqos %{buildroot}/%{_bindir}

install -d %{buildroot}/%{_mandir}/man8
install -m 0644 %{_builddir}/%{githubfull}/pqos.8 %{buildroot}/%{_mandir}/man8

install -d %{buildroot}/%{_licensedir}/%{name}-%{version}
install -m 0644 %{_builddir}/%{githubfull}/LICENSE %{buildroot}/%{_licensedir}/%{name}-%{version}
%endif

%files
%{_bindir}/pqos
%{_mandir}/man8/pqos.8.gz

%if "%{githubver}" == "%{ver015}" || "%{githubver}" == "%{ver0151}"
%{_bindir}/rdtset
%{_mandir}/man8/rdtset.8.gz
%if "%{githubver}" == "%{ver0151}"
%{_libdir}/libpqos-0.1.6.so
%else
%{_libdir}/libpqos-0.1.5.so
%endif
%endif

%{!?_licensedir:%global license %%doc}
%license %{_licensedir}/%{name}-%{version}/LICENSE
%doc ChangeLog README

%changelog
* Tue Feb 14 2017 Aaron Hetherington <aaron.hetherington@intel.com> 0.1.5-1
- new release

* Mon Oct 17 2016 Aaron Hetherington <aaron.hetherington@intel.com> 0.1.5
- new release

* Tue Apr 19 2016 Tomasz Kantecki <tomasz.kantecki@intel.com> 0.1.4-3
- global typo fix
- small edits in the description

* Mon Apr 18 2016 Tomasz Kantecki <tomasz.kantecki@intel.com> 0.1.4-2
- LICENSE file added to the package

* Thu Apr 7 2016 Tomasz Kantecki <tomasz.kantecki@intel.com> 0.1.4-1
- initial version of the package
