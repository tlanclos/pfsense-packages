<?php
/* $Id$ */
/*
	spamd.inc
	part of the SpamD package for pfSense
	Copyright (C) 2008 Scott Ullrich
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.

*/

if(!function_exists("filter_configure")) 
	require_once("filter.inc");

function sync_package_spamd() {
	global $config, $g;
	$pf_version = substr(trim(file_get_contents("/etc/version")),0,3);
	if ($pf_version != "2.2") {
		$localpath = "/usr/local";
	} else {
		$ARCH = php_uname("m");
		$localpath = "/usr/pbi/spamd-$ARCH/local";
	}
	
	conf_mount_rw();
	config_lock();
	$fd = fopen("/etc/spamd.conf","w");
	/* all header */
	fwrite($fd, "all:\\\n\t:whitelist:blacklist");
	if($config['installedpackages']['spamdsources']['config']) {
		foreach($config['installedpackages']['spamdsources']['config'] as $spamd) {
			if($spamd['providername']) {
				fwrite($fd, ":" . remove_spaces($spamd['providername']));
			}
		}
	}
	fwrite($fd, ":\n\n");
	fwrite($fd, "whitelist:\\\n");
	fwrite($fd, "\t:method=file:\\\n");
	fwrite($fd, "\t:white:\\\n");
	fwrite($fd, "\t:file=/var/db/whitelist.txt:\n");
	fwrite($fd, "\n");
	fwrite($fd, "blacklist:\\\n");
	fwrite($fd, "\t:black:\\\n");
	fwrite($fd, "\t:msg=\"Sorry, you spammed us before.\":\\\n");
	fwrite($fd, "\t:method=file:\\\n");
	fwrite($fd, "\t:file=/var/db/blacklist.txt:\n\n");
	log_error("Looping through each item and write out its configuration");
	/* loop through each item and write out its configuration */
	if($config['installedpackages']['spamdsources']['config'] != "") {
		foreach($config['installedpackages']['spamdsources']['config'] as $spamd) {
			if(remove_spaces($spamd['providername'])) {
				if($spamd['providername']) {
					fwrite($fd, remove_spaces($spamd['providername']) . ":\\\n");
					fwrite($fd, "\t:" . remove_spaces($spamd['providertype']) . ":\\\n");
					fwrite($fd, "\t:msg=\"" . rtrim($spamd['rejectmessage']) . "\":\\\n");
					fwrite($fd, "\t:method=" . remove_spaces($spamd['providermethod']) . ":\\\n");
					$providerurl = $spamd['providerurl'];
					$providerurl = str_replace("http://", "", $providerurl);
					$providerurl = str_replace("https://", "", $providerurl);
					fwrite($fd, "\t:file=" . remove_spaces($providerurl) . ":\n\n");
				}
			}
		}
	}
	fclose($fd);
		log_error("Creating /var/db/whitelist.txt");
	$fd = fopen("/var/db/whitelist.txt","w");
	if($config['installedpackages']['spamdwhitelist']['config'] != "") {
	    foreach($config['installedpackages']['spamdwhitelist']['config'] as $spamd) {
			if($spamd['ip'])
				fwrite($fd, $spamd['ip'] . "\n");
		}
	}
	fclose($fd);
	$passtime = "5";
	$greyexp = "4";
	$whiteexp = "864";
	$identifier = "";
	$maxcon = "";
	$maxblack = "";
	$stuttersecs = "";
	$delaysecs = "";
	log_error("Looping through spamdsettings");
	if($config['installedpackages']['spamdsettings']['config']) {
		foreach($config['installedpackages']['spamdsettings']['config'] as $ss) {
			if($ss['nextmta'] <> "")
				$nextmta = $ss['nextmta'];
			if($ss['greylistingparms'] <> "")
				$passtime = " -G " . $ss['greylistingparms'];
			if($ss['identifier'] <> "")
				$identifier = " -n \"" . $ss['identifier'] . "\"";
			// Default is greylisting, turn on blacklisting if not checked.
			if($ss['greylisting'] <> "on")
				$greylisting = " -b";
			if($ss['maxblack'] <> "")
				$maxblack = " -B " . $ss['maxblack'];
			if($ss['maxcon'] <> "")
				$maxcon = " -c " . $ss['maxcon'];
			if($ss['stuttersecs'] <> "")
				$stuttersecs = " -S " . $ss['stuttersecs'];
			if($ss['delaysecs'] <> "")
				$delaysecs = " -s " . $ss['delaysecs'];
			if($ss['window'] <> "")
				$window = " -w " . $ss['window'];
			if($ss['passtime'] <> "")
				$passtime = $ss['passtime'];
			if($ss['greyexp'] <> "")
				$greyexp = $ss['greyexp'];
			if($ss['whiteexp'] <> "")
				$whiteexp = $ss['whiteexp'];
		}
	}
	$greyparms = " -G {$passtime}:{$greyexp}:{$whiteexp}";
	$start = "if [ `mount | grep -v grep | grep fdescfs | wc -l` -lt 1 ]; then \n" .
		 "/sbin/mount -t fdescfs fdescfs /dev/fd\n" .
		 "fi\n" .
	     "/usr/local/sbin/spamd-setup -d &\n" .
		 "/sbin/pflogd &\n" .
		 "$localpath/libexec/spamd {$greyparms}{$identifier}{$greylisting}{$maxcon}{$maxblack}{$window}{$replysmtperror} 127.0.0.1 &\n" .
		 "$localpath/libexec/spamlogd\n";
	$stop  = "/usr/bin/killall spamd-setup\n" .
		 "/usr/bin/killall spamlogd\n" .
		 "/usr/bin/killall spamd\n" .
		 "/usr/bin/killall pflogd\n" .
		 "sleep 2";
	log_error("Writing rc_file");		
	write_rcfile(array(
				"file" => "spamd.sh",
				"start" => $start,
				"stop" =>  $stop
			    )
	);
	log_error("Installing CRON");	
	spamd_install_cron(true);
	log_error("Mounting RO");	
	conf_mount_ro();
	log_error("Unlocking config");	
	config_unlock();
	log_error("Restart cron");	
	mwexec("killall -HUP cron");
	log_error("Setting up spamd.conf symlink");	
	unlink_if_exists("$localpath/etc/spamd/spamd.conf");
	symlink("/etc/spamd.conf", "$localpath/etc/spamd/spamd.conf");
	
	log_error("Stopping spamd");
	mwexec("/usr/local/etc/rc.d/spamd.sh stop");
	sleep(1);
	log_error("Starting spamd");
	mwexec_bg("/usr/local/etc/rc.d/spamd.sh start");
	log_error("Reconfiguring filter");
	filter_configure();
	log_error("SpamD setup completed");	
}

function sync_package_spamd_whitelist() {
	global $config;
	conf_mount_rw();
	config_lock();
	/* write out ip to the whitelist db */
	$fd = fopen("/var/db/whitelist.txt","w");
	if($config['installedpackages']['spamdwhitelist']['config'] != "") {
		foreach($config['installedpackages']['spamdwhitelist']['config'] as $spamd) {
			fwrite($fd, $spamd['ip'] . "\n");
		}
	}
	fclose($fd);
	/* signal a reload of all files */
	mwexec("/usr/bin/killall -HUP spamlogd");
	mwexec("/sbin/pfctl -t spamd-white -T add {$spamd['ip']}");
	conf_mount_ro();
	config_unlock();
}

function spamd_generate_rules($type) {
	global $config;

	$natrules = "";
	switch($type) {
	case 'nat':
		$natrules .= "\n# spam table \n";
                $wanif = get_real_interface("wan");
                $natrules .= "table <whitelist> persist\n";
                $natrules .= "table <blacklist> persist\n";
                $natrules .= "table <spamd> persist\n";
                if(file_exists("/var/db/whitelist.txt"))
                        $natrules .= "table <spamd-white> persist file \"/var/db/whitelist.txt\"\n";
                $natrules .= "rdr pass on {$wanif} proto tcp from <blacklist> to port smtp -> 127.0.0.1 port spamd\n";
                $natrules .= "rdr pass on {$wanif} proto tcp from <spamd> to port smtp -> 127.0.0.1 port spamd\n";
                $natrules .= "rdr pass on {$wanif} proto tcp from !<spamd-white> to port smtp -> 127.0.0.1 port spamd\n";
                if($config['installedpackages']['spamdsettings']['config'])
                        foreach($config['installedpackages']['spamdsettings']['config'] as $ss)
                                $nextmta = $ss['nextmta'];
                if($nextmta <> "") {
                        $natrules .= "rdr pass on {$wanif} proto tcp from <spamd-white> to port smtp -> {$nextmta} port smtp\n";
                }

		break;
	}
	
	return $natrules;
}

function remove_spaces($string) {
	$string = str_replace(" ", "", $string);
	return $string;
}

function sync_spamd_config_to_backup() {
	global $g, $config;
	if(is_array($config['installedpackages']['carpsettings']['config'])) {
		foreach($config['installedpackages']['carpsettings']['config'] as $carp) {
			if($carp['synchronizetoip'] != "" ) {
				$synctoip = $carp['synchronizetoip'];
				$password = $carp['password'];
				if($config['system']['username'])
					$username = $config['system']['username'];
				else 
					$username = "admin";
			}
		}
	}
	if($synctoip and $password) {
		if($config['system']['webgui']['protocol'] != "") {
			$synchronizetoip = $config['system']['webgui']['protocol'];
			$synchronizetoip .= "://";
		}
		$port = $config['system']['webgui']['port'];
		/* if port is empty lets rely on the protocol selection */
		if($port == "") {
			if($config['system']['webgui']['protocol'] == "http") {
				$port = "80";
			} else {
				$port = "443";
			}
		}
		$params = array(XML_RPC_encode($password),XML_RPC_encode($xml));
		/* create files to sync array */
		$filetosync = array("/var/db/spamd", "/var/db/whitelist.txt");
		/* loop through files to sync list and sync them up */
		foreach($filetosync as $f2s) {
			$f2c_contents = file_get_contents($f2s);
			xmlrpc_sync_file($url, $password, $f2s, $f2c_contents, $port);
		}
		/* signal remote process config reload */
		xmlrpc_exec_shell($url, $password, "/usr/bin/killall -HUP spamlogd", $port);
		xmlrpc_exec_shell($url, $password, "/usr/bin/killall -HUP spamd", $port);
	}
}

function custom_php_install_command() {
	global $config, $g;
	system("touch /var/db/whitelist.txt");
	system("touch /var/db/blacklist.txt");
	sync_package_spamd();
	conf_mount_rw();
	exec("/usr/sbin/pw usermod _spamd -g proxy -G _spamd,proxy");
	exec("/bin/cp /usr/local/etc/spamd/spamd.conf.sample /usr/local/etc/spamd/spamd.conf");
	conf_mount_ro();
}

function custom_php_deinstall_command() {
	global $config, $g;
	conf_mount_rw();
	exec("killall -9 spamd");
	unlink_if_exists("/usr/local/pkg/pf/spamd_rules.php");
	unlink_if_exists("/usr/local/www/spamd_rules.php");
	unlink_if_exists("/usr/local/etc/rc.d/spamd.sh");
	spamd_install_cron(false);
	conf_mount_ro();
	filter_configure();
}

function spamd_install_cron($should_install) {
	global $config, $g;
	$is_installed = false;
	if(!$config['cron']['item'])
		return;
	$x=0;
	foreach($config['cron']['item'] as $item) {
		if(strstr($item['command'], "spamd-setup")) {
			$is_installed = true;
			break;
		}
		$x++;
	}
	switch($should_install) {
		case true:
			if(!$is_installed) {
				$cron_item = array();
				$cron_item['minute'] = "*/120";
				$cron_item['hour'] = "*";
				$cron_item['mday'] = "*";
				$cron_item['month'] = "*";
				$cron_item['wday'] = "*";
				$cron_item['who'] = "root";
				$cron_item['command'] = "/usr/bin/nice -n20 /usr/local/sbin/spamd-setup";		
				$config['cron']['item'][] = $cron_item;
				write_config("Installed SPAMD crontab entries.");
				configure_cron();
			}
		break;
		case false:
			if($is_installed == true) {
				if($x > 0) {
					unset($config['cron']['item'][$x]);
					write_config();
				}
				configure_cron();
			}
		break;
	}
}

function spamd_validate_input($post, &$input_errors) {
	global $config, $g;
	$nextmta = str_replace("$", "", $post['nextmta']);
	if(stristr($nextmta, "{")) {
		/* item is an alias, make sure the name exists */
		$nextmta = str_replace("$", "", $nextmta);
		$found = false;
		if($config['aliases']['alias']) {
			foreach($config['aliases']['alias'] as $alias) {
				if($alias['name'] == $nextmta) {
					$found = true;
				}
			}
		}
		if($found == false)
			$intput_errors = "Could not locate alias named " . htmlentities($nextmta);
	}
}

?>
