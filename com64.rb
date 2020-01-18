#!/usr/bin/env ruby
# coding: utf-8

# Proxy for this gdbstub + EverDrive-64
# usage: ruby $0 /dev/ttyUSB0 [port]

require 'io/console'
require 'socket'

def read_whole_timeout io, size, timeout
	buf = ''.b
	while buf.size < size
		rs, _, _ = IO.select([io], nil, nil, timeout)
		rs.nil? or rs.size == 0 and return buf
		buf += io.readpartial size - buf.size
	end
	return buf
end

def interact remote, local, cut=false
	loop {
		rs, _, es = IO.select([remote, local[:r]], nil, [remote, local[:r]])
		if not es.empty?
			$stderr.puts es.inspect
			break
		end
		if rs.include?(remote)
			r = read_whole_timeout(remote, 512, 0.5)
			cut and r.sub!(/\0*$/n, '')
			cut and $stderr.puts "> #{r.inspect}"
			local[:w].write(r)
		end
		if rs.include?(local[:r])
			local[:r].eof? and break
			r = local[:r].readpartial(2048)
			if not cut and r[0] == '!'
				r = '$' + r[1..-2] + '#' + '%02x' % [r[1..-2].bytes.inject(&:+) & 0xFF]
			end
			cut and $stderr.puts "< #{r.inspect}"
			r = r.ljust((r.size + 511) & -512, "\0")
			remote.write(r)
		end
	}
end

File.open(ARGV[0], 'r+b') { |com|
	com.raw!
	com.sync = true

	if ARGV[1]
		ARGV[1] =~ /\A(?:(.+):)?(.+)\z/
		addr = $1 || '127.0.0.1'
		port = $2.to_i.abs.to_s
		verbose = $2.to_i < 0
		TCPServer.open(addr, port) { |ss|
			loop {
				puts "listening #{addr}:#{port}"
				s = ss.accept
				puts "accept from #{s.peeraddr.values_at(3,1).inspect}"
				interact com, {r: s, w: s}, verbose
				puts "disconnected"
			}
		}
	else
		$stdout.puts "com64 ready"
		$stdout.sync = true
		interact com, {r: $stdin, w: $stdout}
	end
}
