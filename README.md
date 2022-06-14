# Unitd

Unit daemon is a simple, fast and modern init system which is mainly focused on an accurate processes supervision.<br/>
The main features are the following:

- multi-threading start
- dependencies and conflicts management
- Restart of the processes providing the history

For this init system, every process is named "**unit**" and will has "**.unit**" as configuration file extension.<br/>
Unlike the others init systems, Unit daemon doesn't run a "**level**" or strike a "**target**".<br/>
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

[**Unit**]														          (required and not repeatable)<br/>
**_Description_** = NetworkManager								(required and not repeatable)<br/>

**_Requires_** = NetworkManager									  (optional and repeatable)<br/>
**_Requires_** = ...<br/>

**_Conflict_** = dhcpcd											      (optional and repeatable)<br/>
**_Conflict_** = ......<br/>

**_Type_** = oneshot												      (optional and not repeatable. If omitted is "daemon")<br/>
**_Restart_** = true												      (optional and not repeatable. If omitted  is "false")<br/>
**_RestartMax_** = num											      (optional and not repeatable. A numeric value major than zero)<br/>

[**Command**]                                   (required and not repeatable)<br/>
**_Run_** = /sbin/NetworkManager									(required and not repeatable)<br/>
**_Stop_** = /sbin/NetworkManager -stop						(optional and not repeatable)<br/>

[**State**]														          (required and not repeatable)<br/>
**_WantedBy_** = multi-user.state									(required and repeatable)<br/>

FIXME Continue....
  
