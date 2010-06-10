require 'rubygems'
require 'builder'
require 'sqlite3'

db = SQLite3::Database.new(ARGV[0])
search = ARGV[1]

xml = Builder::XmlMarkup.new(:target => STDOUT, :indent => 2)
xml.instruct!(:xml, :encoding => "iso-8859-1")
xml.declare!(:DOCTYPE, :nzb, :PUBLIC, "-//newzBin//DTD NZB 1.0//EN", "http://www.newzbin.com/DTD/nzb/nzb-1.0.dtd")
xml.nzb(:xmlns => "http://www.newzbin.com/DTD/2003/nzb") do |nzb|
  files = {}
  all_group_ids = []
  db.execute("SELECT group_id, subject, message_id FROM articles WHERE subject LIKE '%#{search}%'") do |row|
    group_id, subject, message_id = row

    if md = subject.match(/"([^"]+)"\s+\((\d+)\/(\d+)\)\s*$/)
      file, part, total = md[1..3]
      if !files[file]
        $stderr.puts "Found file: #{file}"
        files[file] = { :num => total, :group_ids => [], :segments => [] }
      end
      files[file][:group_ids] << group_id  if !files[file][:group_ids].include?(group_id)
      files[file][:segments]  << [part, message_id.sub(/^<?(.+)>?$/, '\1')]
      all_group_ids << group_id   if !all_group_ids.include?(group_id)
    else
      $stderr.puts "Couldn't parse subject: #{subject}"
    end
  end

  all_groups = db.execute("SELECT id, name FROM groups WHERE id IN (#{all_group_ids.join(", ")})")

  files.each_pair do |filename, info|
    nzb.file(:subject => "#{filename} (1/#{info[:num]})") do |file|
      file.groups do |groups|
        info[:group_ids].each do |group_id|
          groups.group(all_groups.assoc(group_id)[1])
        end
      end

      file.segments do |segments|
        info[:segments].each do |segment|
          segments.segment(segment[1], :number => segment[0])
        end
      end
    end
  end
end
STDOUT.flush
