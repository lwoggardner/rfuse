require 'spec_helper'
require 'pathname'
require 'tempfile'
require 'sys/filesystem'

describe RFuse::Fuse do

    let(:dir_stat) { RFuse::Stat.directory(0444) }
    let(:file_stat) { RFuse::Stat.file(0444) }
    let!(:mockfs) { m = mock("fuse"); m.stub(:getattr).and_return(nil); m }
    let(:mountpoint) { tempmount() }

    context "links" do
        it "should create and resolve symbolic links"
        it "should create and resolve hard links"

    end

    context "directories" do
        it "should make directories" do

            mockfs.stub(:getattr).and_return(nil)
            mockfs.stub(:getattr).with(anything(),"/aDirectory").and_return(nil,dir_stat)
            mockfs.should_receive(:mkdir).with(anything(),"/aDirectory",anything())

            with_fuse(mountpoint,mockfs) do
                Dir.mkdir("#{mountpoint}/aDirectory")
            end
        end

        it "should list directories" do

            mockfs.should_receive(:readdir) do | ctx, path, filler,offset,ffi |
                filler.push("hello",nil,0)
                filler.push("world",nil,0)
            end

            with_fuse(mountpoint,mockfs) do
                entries = Dir.entries(mountpoint)
                entries.size.should == 2
                entries.should include("hello")
                entries.should include("world")
            end
        end
    end

    context "permissions" do
        it "should process chmod" do
            mockfs.stub(:getattr).with(anything(),"/myPerms").and_return(file_stat)

            mockfs.should_receive(:chmod).with(anything(),"/myPerms",file_mode(0644))

            with_fuse(mountpoint,mockfs) do
                File.chmod(0644,"#{mountpoint}/myPerms").should == 1
            end
        end
    end

    context "timestamps" do


        it "should support stat with subsecond resolution" do
            testns = Tempfile.new("testns")
            stat = File.stat(testns.path)

            # so if this Ruby has usec resolution for File.stat
            # we'd expect to skip this test 1 in 100,000 times...
            no_usecs = (stat.mtime.usec == 0)

            if no_usecs
                puts "Skipping due to no usec resolution for File.stat"
            else
                atime,mtime,ctime =  usec_times(60,600,3600)

                file_stat.atime = atime
                file_stat.mtime = mtime
                file_stat.ctime = ctime

                # ruby can't set file times with ns res, o we are limited to usecs
                mockfs.stub(:getattr).with(anything(),"/nanos").and_return(file_stat)

                with_fuse(mountpoint,mockfs) do
                    stat = File.stat("#{mountpoint}/nanos")
                    stat.atime.usec.should == atime.usec
                    stat.atime.should == atime
                    stat.ctime.should == ctime
                    stat.mtime.should == mtime
                end
            end
        end

        it "should set file access and modification times subsecond resolution" do
            atime,mtime = usec_times(60,600)

            atime_ns = (atime.to_i * (10**9)) + (atime.nsec)
            mtime_ns = (mtime.to_i * (10**9)) + (mtime.nsec)

            mockfs.stub(:getattr).with(anything(),"/usec").and_return(file_stat)
            mockfs.should_receive(:utimens).with(anything,"/usec",atime_ns,mtime_ns)

            with_fuse(mountpoint,mockfs) do
                File.utime(atime,mtime,"#{mountpoint}/usec").should == 1
            end
        end
        it "should set file access and modification times" do

            atime = Time.now()
            mtime = atime + 1

            mockfs.stub(:getattr).with(anything(),"/times").and_return(file_stat)
            mockfs.should_receive(:utime).with(anything(),"/times",atime.to_i,mtime.to_i)

            with_fuse(mountpoint,mockfs) do
                File.utime(atime,mtime,"#{mountpoint}/times").should == 1
            end
        end

    end

    context "file io" do

        it "should create files" do

            mockfs.stub(:getattr).with(anything(),"/newfile").and_return(nil,file_stat)
            mockfs.should_receive(:mknod).with(anything(),"/newfile",file_mode(0644),0,0)

            with_fuse(mountpoint,mockfs) do
                File.open("#{mountpoint}/newfile","w",0644) { |f| }
            end
        end

        # ruby doesn't seem to have a native method to create these
        # maybe try ruby-mkfifo
        it "should create special device files"

        it "should read files" do

            file_stat.size = 12
            mockfs.stub(:getattr).with(anything(),"/test").and_return(file_stat)


            reads = 0
            mockfs.stub(:read) { |ctx,path,size,offset,ffi|
                reads += 2
                "hello\000world\000"[offset,reads]
            }

            with_fuse(mountpoint,mockfs) do
                File.open("#{mountpoint}/test") do |f|
                    val = f.gets
                    val.should == "hello\000world\000"
                end
            end
        end

        it "should read over null characters in a real file" do
            file_stat.size = 2
            File.open("/tmp/nulltest","w") { |f| f.print "\000\000" }

            mockfs.stub(:getattr).with(anything(),"/testnull").and_return(file_stat)

            mockfs.stub(:read) { |ctx,path,size,offset,ffi|
                IO.read("/tmp/nulltest",size,offset)
            }

            with_fuse(mountpoint,mockfs) do
                File.open("#{mountpoint}/testnull") do |f|
                    val = f.gets
                    val.should == "\000\000"
                    val.size.should == 2
                end
            end
        end

    end

    context "filesystem statistics" do

        let (:statvfs) {
            RFuse::StatVfs.new(
                :bsize => 2048,
                "frsize" => 1024,
                "blocks" =>  9999,
                "bfree" => 8888,
                "bavail" => 7777,
                "files" => 6000,
                "ffree" => 5555)
        }

        it "should report filesystem statistics" do
            mockfs.stub(:getattr).with(anything(),"/test").and_return(dir_stat)
            mockfs.stub(:getattr).with(anything(),"/test/statfs").and_return(file_stat)

            mockfs.should_receive(:statfs).with(anything(),"/test/statfs").and_return(statvfs)

            #also exercise StatVfs
            statvfs.f_files=6666

            with_fuse(mountpoint,mockfs) do

                results = Sys::Filesystem.stat("#{mountpoint}/test/statfs")
                results.block_size.should == 2048
                results.fragment_size.should == 1024
                results.blocks.should == 9999
                results.blocks_available.should == 7777
                results.blocks_free.should == 8888
                results.files.should == 6666
                results.files_available == 5555
            end
        end
    end

    context "exceptions" do

        it "should capture exceptions appropriately" do

            mockfs.should_receive(:getattr).with(anything(),"/exceptions").and_raise(RuntimeError)

            with_fuse(mountpoint,mockfs) do
                begin
                    File.stat("#{mountpoint}/exceptions")
                    raise "should not get here"
                rescue Errno::ENOENT
                    #all good
                end
            end
        end

        it "should pass Errno exceptions through" do
            mockfs.should_receive(:getattr).with(anything(),"/exceptions").and_raise(Errno::EPERM)

            with_fuse(mountpoint,mockfs) do
                begin
                    File.stat("#{mountpoint}/exceptions")
                    raise "should not get here"
                rescue Errno::EPERM
                    #all good
                end
            end
        end

    end
end
