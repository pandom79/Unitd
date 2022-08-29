# Unitd

Unit daemon is a simple, fast and modern init system which is mainly focused on an accurate processes supervision.<br/>
The main features are the following:

- multi-threading start
- dependencies and conflicts management
- Restart of the processes providing the history


### Dependencies

- [Wrapper](https://github.com/pandom79/wrapper) library
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

The **init** and **final** states are excluded by normal units handling. In these states are only present the units which "initialize" and "finalize" the system. An example, could be the mounting and unmounting the disk, etc... Normally, the final user should not consider these states.

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
**Meson build option (DAEMON_SIGNAL_TIME)**<br>
It represents the time (milliseconds) between the execution of the daemon and the signal to the units which depend on it.<br>
The higher its value, the more likely it is that units that depend on it will start successfully.<br>
Consequently, the startup time will also be longer.<br>
Conversely, a smaller value will reverse the one just said.<br>
In this case, the units that are most likely to fail may require setting the **Restart** or **RestartMax** properties to fix the problem.<br>
To get the fastest boot time you will need to set this value to zero (**FAST BOOT**).<br>
Its default value is 200ms which seems a reasonable time.<br>
Note that if you have an oneshot unit that takes 3-4 seconds (just an example) to do its job,<br>
you will most likely not notice any change in boot time regardless of this value.<br>
I'd recommend to properly evaluate your use case before to change this value to avoid to get a more "dirty" starting<br>
without some advantages about the boot time.

**Why ?**<br>
This value affects on the speed and starting behaviour.<br>
For this reason, I've chosen to make it a your choice rather than a fixed parameter.<br>
Could be there some contexts (Container, VM...etc) where in wait for 200 ms (more or less) is basically useless.<br>

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

As you can see, the first boot will load the **multi-user-net** state.<br/>
Every state requires at least one unit which is normally a terminal.<br/>
For this reason, I suggest you to copy this [unit](https://github.com/pandom79/Unitd/blob/master/src/extra/units/agetty-1.unit) in **/usr/lib64/unitd/units**.<br>
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
The units created from user will be placed in **$HOME/.config/unitd/units**.

### Unit configuration file

```
[Unit]                              (required and not repeatable)
Description = NetworkManager	    (required and not repeatable)

Requires = dbus                     (optional and repeatable)
Requires = ...

Conflict = dhcpcd                   (optional and repeatable)<br/>
Conflict = ...

Type = oneshot|daemon               (optional and not repeatable. If omitted is "daemon")
Restart = true|false                (optional and not repeatable. If omitted  is "false")
RestartMax = num                    (optional and not repeatable. A numeric value greater than zero)

[Command]                           (required and not repeatable)
Run = /sbin/NetworkManager          (required and not repeatable)
Stop = /sbin/NetworkManager -stop   (optional and not repeatable)

[State]                             (required and not repeatable)
WantedBy = multi-user-net           (required and repeatable for system instance)
WantedBy = ...
WantedBy = user                     (required and not repeatable for user instance)
```
Please note, if **Restart** and **RestartMax** properties are defined then Restart property will be ignored.<br>

### Unitctl 

The units handling is possible via **unitctl** command.<br/>
Run ```unitctl --help or -h``` to know the usage for the system instance.<br/>
Run ```unitctl --user --help or -uh``` to know the usage for the user instance.<br/>


### Note

The initialization and finalization units are based on [Void Linux Runit](https://github.com/void-linux/void-runit)<br/>
Thanks to Void linux Team for providing them.<br/>
Another thanks to Leah Neukirchen which provides "zzz" script for the hibernate/suspend management.<br/>

