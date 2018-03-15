#!/usr/bin/env ruby
# coding: utf-8

# USB Loader for EverDrive-64
# usage: ruby $0 /dev/ttyUSB0 /path/to/rom.z64

require 'io/console'

def read_whole_timeout io, size, timeout
	buf = ''.b
	while buf.size < size
		rs, _, _ = IO.select([io], nil, nil, timeout)
		rs.nil? or rs.size == 0 and return buf
		buf += io.readpartial size - buf.size
	end
	return buf
end

def ensure_resp_ok io, op=nil
	r = read_whole_timeout io, 512, 0.5
	r[3] == 'k' or raise "Unexpected #{op} command response: #{r.inspect}"
	#$stderr.puts r.inspect
end

def send_command io, cmd
	io.write cmd.ljust(512, "\0")
end

def sendrecv_command io, cmd, op=nil
	send_command io, cmd
	return ensure_resp_ok io, op
end

$stdout.sync = true
File.open(ARGV[0], 'r+b') { |com|
	com.raw!
	com.sync = true
	File.open(ARGV[1], 'r+b') { |f|
		$stdout.print 'Testing...'
		sendrecv_command com, 'CMDT', 'test'
		$stdout.print 'OK, '

		fsz = File.size ARGV[1]
		if fsz < 2*1024*1024
			$stdout.print 'Filling...'
			sendrecv_command com, 'CMDF', 'fill2M'
			$stdout.print 'OK, '
		end

		$stdout.puts

		addr = 0
		size = (fsz + 65535) / 65536 * 65536
		tstart = Time.now
		send_command com, ['CMDW', addr/2048, size/512].pack("a*nn")
		(0...(size/32768)).each { |i|
			com.write (f.read(32768) || ''.b).ljust(32768, "\0")
			$stderr.printf "\r%d/%d %.2f", (i+1)*32768, size, Time.now - tstart
		}
		com.write 'CMDS'.ljust(512, "\0")
		$stderr.puts
	}
}
