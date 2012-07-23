require 'spec_helper'

describe RFuse::Fuse do

    it "should throw exception for bad options"

    it "should list directories" do
         
        mockfs = mock("fuse")

        spec_pid = Process.pid

        mockfs.should_receive(:readdir) do | ctx, path, filler,offset,ffi | 
            filler.push("hello",nil,0)
            filler.push("world",nil,0)
        end

        with_fuse("/tmp/rfuse-spec",mockfs) do
            entries = Dir.entries("/tmp/rfuse-spec")
            entries.size.should == 2
            entries.should include("hello")
            entries.should include("world")
        end
    end

    it "should read files" do

        mockfs = mock("fuse")

        file_stat = RFuse::Stat.file(0444,:size => 11)

        mockfs.stub(:getattr) { | ctx, path|
            case path 
            when "/test"
                file_stat
            else
                raise Errno::ENOENT 
            end
            
        }

        reads = 0
        mockfs.stub(:read) { |ctx,path,size,offset,ffi|
            reads += 2
            "hello world"[offset,reads]
        }

        with_fuse("/tmp/rfuse-spec",mockfs) do
            File.open("/tmp/rfuse-spec/test") do |f|
                f.gets.should == "hello world"
            end
        end

    end

    it "should retain file handles over GC" do
        mockfs = mock("fuse")

        file_stat = RFuse::Stat.file(0444,:size => 11)

        mockfs.stub(:getattr) { | ctx, path|
            case path 
            when "/one","/two"
                file_stat
            else
                raise Errno::ENOENT 
            end
            
        }

        open_files = {}
        mockfs.stub(:open) { |ctx,path,ffi|
            ffi.fh = Object.new()
            open_files[path] = ffi.fh.object_id
        }

        mockfs.stub(:read) { |ctx,path,size,offset,ffi|
            GC.start()
            open_files[path].should == ffi.fh.object_id
            "hello world"
        }

        with_fuse("/tmp/rfuse-spec",mockfs) do
            f1 = File.new("/tmp/rfuse-spec/one")
            f2 = File.new("/tmp/rfuse-spec/two")

            f1.gets.should == "hello world" 
            f2.gets.should == "hello world"

            f1.close()
            f2.close()
        end
    end

    
end
