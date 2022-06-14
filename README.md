# Unitd

Unit daemon is a simple, fast and modern init system which is mainly focused on an accurate processes supervision.
The main features are the following:

- multi-threading start
- dependencies and conflicts management
- Restart of the processes providing the history

For this init system, every process is named "**unit**" and will has "**.unit**" as configuration file extension.

Unlike the others init systems, Unit daemon doesn't run a "**level**" or strike a "**target**". 
It brings the system in a determine "**state**".

The available states are:

- init
- poweroff
- single-user
- multi-user
- multi-user-net
- custom
- graphical
- reboot
- final

The **init** and **final** states are excluded by normal units handling. In these states are only present the units which "initialize" and "finalize" the system. An example, could be the mounting and unmounting the disk, etc... Normally, the final user should not consider these states.


### Unit configuration file

[Unit]														(required)
Description = NetworkManager								(required)

Requires = NetworkManager									(optional and repeatable)
Requires = ...

Conflict = dhcpcd											(optional and repeatable)
Conflict = ......

Type = oneshot												(optional and not repeatable. If omitted is daemon)
Restart = true												(optional and not repeatable. If omitted  is false)
RestartMax = num											(optional and not repeatable. A numeric value major than zero)

[Command]
run = /sbin/NetworkManager									(required)
stop = /sbin/NetworkManager -stop							(optional)

[State]														(required)
WantedBy = multi-user.state									(required and repeatable)

  
