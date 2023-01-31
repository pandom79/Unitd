# <img src="artwork/unitd-logo.svg" alt="Unitd init system" height="60"/>

Unit daemon is a simple, fast and modern init system which is mainly focused on an accurate processes supervision.<br/>
The main features are the following:

- multi-threading start
- dependencies and conflicts management
- Restart of the processes providing the history

Run ```unitd --help or -h``` to know the usage.<br/>
Run ```man unitd``` to consult the manual page.<br/>

### Overview
- <a href="#dependencies">Dependencies</a>
- <a href="#states">States</a>
- <a href="#build-instructions">Build instructions</a>
- <a href="#unit-configuration-file">Unit configuration file</a>
- <a href="#timers">Timers</a>
- <a href="#timer-unit-configuration-file">Timer unit configuration file</a>
- <a href="#path-unit">Path unit</a>
- <a href="#path-unit-configuration-file">Path unit configuration file</a>
- <a href="#how-to-configure-the-units">How to configure the units?</a>
- <a href="#unitctl">Unitctl</a>
- <a href="#unitlogd-and-unitlogctl">Unitlogd and Unitlogctl</a>
- <a href="#note">Note</a>

### Dependencies

- [Wrapper](https://github.com/pandom79/wrapper) library
- A POSIX thread library
- A POSIX shell
- A POSIX awk
- procps-ng (needs pkill -s0,1)

### States

For this init system, every process is named "**unit**" and will has "**.unit**" as configuration file extension.<br/>
Unlike the others init systems, Unit daemon doesn't run a "**level**" or strike a "**target**".<br/>
It brings the system in a "**state**".<br/>

For the system instance, the available states are:
- init
- poweroff
- single-user
- multi-user
- multi-user-net
- custom
- graphical
- reboot
- final

For the user instance we have only one state: 
- user

The **init** and **final** states are excluded by normal units handling.<br>
In these states are only present the units which "initialize" and "finalize" the system.<br>
An example could be the mounting and unmounting the disk, etc...<br>
Normally, the final user should not consider these states.

### Build instructions

Download the release from [here](https://github.com/pandom79/Unitd/releases) and uncompress it.<br/>

Run the following steps as root user:</br>
```
cd Unitd
meson setup build
cd build
meson configure -DOS_NAME="Slackware Linux"
meson compile
meson install
```
For the other build options, please consult **meson_options.txt**.<br>
After that, you should regenerate the icons and mime types cache.<br>
To do that, on slackware, according the default options, I run :
```
gtk-update-icon-cache -f /usr/share/icons/*
update-mime-database /usr/share/mime
```
Please note that if you use a different distro or build options then you should check the paths and command names.

According the default options, you should see the following folders:<br>

- /usr/lib64/unitd/units        (system units path)
- /usr/lib64/unitd/units/user   (user units path)
- /etc/unitd/units              (system units enabling path)
    - custom.state
    - default.state -> /etc/unitd/units/multi-user-net.state
    - graphical.state
    - multi-user-net.state
    - multi-user.state
    - poweroff.state
    - reboot.state
    - single-user.state

As you can see, the first boot will bring the system into **multi-user-net** state.<br/>
Every state requires at least one unit which is normally a terminal.<br/>
For this reason, I suggest you to copy this [unit](https://github.com/pandom79/Unitd/blob/master/units/agetty-1.unit) in **/usr/lib64/unitd/units**.<br>
After that, check the **Run** property of the **Command** section works and enable this unit via the following command:<br>
```
ln -s /usr/lib64/unitd/units/agetty-1.unit /etc/unitd/units/multi-user-net.state/.
```
You should repeat this operation for all the states except **reboot** and **poweroff**.<br/>

About the user instance, you should see the following folders:<br>

- $HOME/.config/unitd/units         (local user units path)
- $HOME/.local/share/unitd/units    (user units enabling path)

Usually, the user units installed from distro packages will be placed in **/usr/lib64/unitd/units/user**.<br/>
Some examples could be **pipewire**, **wireplumbler** and so on.<br/>
The units created by user will be placed in **$HOME/.config/unitd/units**.

### Unit configuration file

```
[Unit]                              (required and not repeatable)
Description = NetworkManager	    (required and not repeatable)

Requires = dbus.unit                (optional and repeatable)
Requires = ...

Conflict = dhcpcd.unit              (optional and repeatable)
Conflict = ...

Type = oneshot|daemon               (optional and not repeatable. If omitted is "daemon")
Restart = true|false                (optional and not repeatable. If omitted  is "false")
RestartMax = num                    (optional and not repeatable. A numeric value greater than zero)

[Command]                           (required and not repeatable)
Run = /sbin/NetworkManager          (required and not repeatable)
Stop = /sbin/NetworkManager -stop   (optional and not repeatable)
Failure = /your/failure/command     (optional and not repeatable)

[State]                             (required and not repeatable)
WantedBy = multi-user-net           (required and repeatable for system instance)
WantedBy = ...
WantedBy = user                     (required and not repeatable for user instance)
```
**Dependencies vs Conflicts**<br>
The dependencies are **uni-directionals**.<br>
That means if the **Unit B** depends on **Unit A** then
we can assert the **Unit A** cannot depends on **Unit B**.<br>
If not, we'll have a block when the system starts because **Unit B** wait for **Unit A** to terminate and the inverse.<br>
Another axample could be **Unit A** depends on **Unit B**, **Unit B** depends on **Unit C** and **Unit C** depends on **Unit A**.<br>
Also this case will fail.<br>
The dependencies have an unique sense and must not come back.<br>
Unlike the dependencies, the conflicts are **bi-directionals** instead.<br>
That means if the **Unit A** has a conflict with **Unit B** then
we can assert the **Unit B** has a conflict with the **Unit A**.<br>
In the conflict case, I'd recommend to set the **Conflict** property in both the units.<br>

**Restart** and **RestartMax**<br>
Please note, if both are defined then Restart property will be ignored.<br>

**Stop**<br>
This property could use the variable **$PID** which can be passed as argument to a custom stop command.<br>
Example:<br>
```Stop = /path/your/command $PID```

**Failure**<br>
In this propery we can set a command which will be executed if the **Run** property command fails.<br>
This property should have to contain an oneshot command.<br>
About the daemon units, the failure command will be also executed if a daemon crashes or signaled for some reason.<br>
A system administrator could want be warned or run some specific tasks if a critical daemon unit unexpectedly fails.<br>
The goal of this property is to satisfy these needs.<br>

### Timers

The timers have **.utimer** as configuration file extension.<br>
The purpose of a timer is to start an unit when a period of time has elapsed which can be set in the timer unit configuration file.<br>
There is an one to one relation between unit and timer.<br>
That means that a timer named **test.utimer** will try to start an unit named **test.unit** when that period has elapsed.<br>
These units cannot run forcing operation.<br>
That means that if **test.unit** has some conflicts then the starting of this unit via the timer will always fail.<br>
These units can be started or restarted with the **reset** option.<br>
The reset option also works with enable/re-enable command only if the run option is set as well.<br>
That will cause a recalculation of the remaining time starting from the current.

### Timer unit configuration file

```
[Unit]                              (required and not repeatable)
Description = NetworkManager	    (required and not repeatable)

Requires = dbus.unit                (optional and repeatable)
Requires = ...

Conflict = dhcpcd.unit              (optional and repeatable)
Conflict = ...

WakeSystem = true|false             (optional and not repeatable. If omitted  is "false")

[Interval]                          (required and not repeatable)
Seconds = num                       (optional and not repeatable. A numeric value greater than zero)
Minutes = num                       (optional and not repeatable. A numeric value greater than zero)
Hours   = num                       (optional and not repeatable. A numeric value greater than zero)
Days    = num                       (optional and not repeatable. A numeric value greater than zero)
Weeks   = num                       (optional and not repeatable. A numeric value greater than zero)
Months  = num                       (optional and not repeatable. A numeric value greater than zero)

[State]                             (required and not repeatable)
WantedBy = multi-user-net           (required and repeatable for system instance)
WantedBy = ...
WantedBy = user                     (required and not repeatable for user instance)
```
**WakeSystem**<br>
The timers configured with **WakeSystem = true** will activate the system in suspension case.<br>
**Interval**<br>
Even if the interval section properties are optionals, at least one criterion must be defined.<br>

### Path unit

The path unit have **.upath** as configuration file extension.<br>
The purpose of a path unit is to start an unit when a file system event occurred.<br>
Tha path units follow the same timers policy about the relation and activation of the unit.<br>

### Path unit configuration file

```
[Unit]                              (required and not repeatable)
Description = NetworkManager	    (required and not repeatable)

Requires = dbus.unit                (optional and repeatable)
Requires = ...

Conflict = dhcpcd.unit              (optional and repeatable)
Conflict = ...

[Path]                              (required and not repeatable)
PathExists = ...                    (optional and not repeatable)
PathExistsGlob = ...                (optional and not repeatable)
PathResourceChanged = ...           (optional and not repeatable)
PathDirectoryNotEmpty = ...         (optional and not repeatable)

[State]                             (required and not repeatable)
WantedBy = multi-user-net           (required and repeatable for system instance)
WantedBy = ...
WantedBy = user                     (required and not repeatable for user instance)
```
**Path**<br>
Even if the path section properties are optionals, at least one path must be defined.<br>
**PathExists**<br>
Watch the mere existence of a file or directory. If the resource specified exists, the related unit will be activated.<br>
**PathExistsGlob**<br>
Works similarly, but checks for the existence of at least one resource matching the globbing pattern specified.<br> 
**PathResourceChanged**<br>
Watch the defined resource changing. If the resource specified changes, the related unit will be activated.<br>
**PathDirectoryNotEmpty**<br>
Watch a directory and activate the related unit whenever it contains at least one resource.

### How to configure the units?

One of the problems which you could have is the failure of a dependency when the system starts due to the speed for which the daemons signal the dependencies.<br>
To fix this problem, you could use the following methods:
1) Set the **Restart** or **RestartMax** property.
2) Use a **check unit** (recommended).

The first method (Restart) will cause a restarting until the unit will properly starts.<br>
The second method (check unit) is a bit more complex instead.<br>
If a dependency exists means that unit needs of "something" of the other unit.<br>
The first step to make is figure out that.<br>
Let's make an example!<br>
Consider two units: *dbus* and *bluetooth*.<br>

*dbus.unit*
```
[Unit]
Description = Dbus

[Command]
Run = set dbus command

[State]
WantedBy = multi-user
WantedBy = multi-user-net
WantedBy = custom
WantedBy = graphical
```

*bluetooth.unit*
```
[Unit]
Description = Bluetooth manager
Requires = dbus.unit

[Command]
Run = set bluetooth command

[State]
WantedBy = multi-user-net
WantedBy = graphical
```
As we know, bluetooth requires dbus but, actually, what does it require?<br>
Why does the first starting fail?<br>
After a little investigation, I figure out that bluetooth wants **/var/run/dbus/system_bus_socket** which is just a socket.<br>
At this point, a check unit can fix the problem.<br>
I created an oneshot unit named *check-dbus* which content could be:

*check-dbus.unit*
```
[Unit]
Description = Check dbus
Requires = dbus.unit
Type = oneshot

[Command]
Run = check-script.sh

[State]
WantedBy = multi-user
WantedBy = multi-user-net
WantedBy = custom
WantedBy = graphical
```
The **check-script.sh** content must only check that the socket **/var/run/dbus/system_bus_socket** is there.<br>
After that, we can change the **Requires** property of the *bluetooth.unit* from *dbus* to *check-dbus*.<br>
At this point, *bluetooth.unit* will start only when that socket is there avoiding the failure<br>
and an eventual restarting as well.<br>
You should consider this method to configure all the units.<br>
The **Restart** or **RestartMax** property should have to be used only when the unit accidentally crashes for some reason.<br>


### Unitctl 

The units handling is possible via **unitctl** command.<br/>
Run ```unitctl --help or -h``` to know the usage for the system instance.<br/>
Run ```unitctl --user --help or -uh``` to know the usage for the user instance.<br/>
Run ```man unitctl``` to consult the manual page.

### Unitlogd and Unitlogctl

Unitlog daemon is an unique and indexed system log.<br/>
Run ```unitlogd --help or -h``` to know the usage.<br/>
Run ```man unitlogd``` to consult the manual page.<br/>

The log handling is possible via **unitlogctl** command.<br/>
Run ```unitlogctl --help or -h``` to know the usage.<br/>
Run ```man unitlogctl``` to consult the manual page.

### Note

The initialization and finalization units are based on [Void Linux Runit](https://github.com/void-linux/void-runit).<br/>
Thanks to Void linux Team for providing them.<br/>
Another thanks to Leah Neukirchen which provides "zzz" script for the hibernate/suspend management.<br/>

