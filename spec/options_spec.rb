require 'spec_helper'
require 'pathname'
require 'tempfile'

describe RFuse do

    let(:dir_stat) { RFuse::Stat.directory(0444) }
    let(:file_stat) { RFuse::Stat.file(0444) }
    let!(:mockfs) { m = mock("fuse"); m.stub(:getattr).and_return(nil); m }
    let(:mountpoint) { tempmount() }

    context "mount options" do
        it "should handle -h" do
            fuse = RFuse::FuseDelegator.new(mockfs,mountpoint,"-h")
            fuse.mounted?.should be_false
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should behave sensibly for bad mountpoint" do
            fuse = RFuse::FuseDelegator.new(mockfs,"bad/mount/point")
            fuse.mounted?.should be_false
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should behave sensibly for bad options" do
            fuse = RFuse::FuseDelegator.new(mockfs,mountpoint,"-eviloption") 
            fuse.mounted?.should be_false
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should handle a Pathname as a mountpoint" do
            fuse = RFuse::FuseDelegator.new(mockfs,Pathname.new(mountpoint))
            fuse.mounted?.should be_true
            fuse.unmount()
        end
    end

    context "#parse_options" do

        it "should detect -h" do
            argv = [ "/mountpoint","-h" ]
            result = RFuse.parse_options(argv)
            
            result[:help].should be_true
            result[:mountpoint].should == "/mountpoint"
            result[:debug].should be_false
        end

        it "should detect -h mixed with -o" do
            argv = [ "/mountpoint","-h", "-o", "debug" ]
            result = RFuse.parse_options(argv)
            
            result[:help].should be_true
            result[:mountpoint].should == "/mountpoint"
            result[:debug].should be_true
        end

        it "should detect debug" do
            argv = [ "/mountpoint","-o","debug" ]
            result = RFuse.parse_options(argv)

            result[:debug].should be_true
            result[:help].should be_false

            argv = [ "/mountpoint","-o","default_permissions,debug" ]
            result = RFuse.parse_options(argv)
            result[:debug].should be_true
        end

        it "should remove local options" do
            argv = [ "/mountpoint","-o","debug,myoption" ]
            
            result = RFuse.parse_options(argv,:myoption)

            result[:debug].should be_true
            result[:myoption].should be_true

            argv[2].should == "debug"
        end

        it "should parse values from options" do
            argv = [ "/mountpoint", "-o", "debug,someopt=somevalue" ]
            result = RFuse.parse_options(argv)

            result[:someopt].should == "somevalue"
        end

        it "should parse values and remove local options" do
            argv = [ "/mountpoint", "-o", "debug,someopt=somevalue" ]
            result = RFuse.parse_options(argv,:someopt)

            result[:someopt].should == "somevalue"
            argv[2].should == "debug"
        end

        it "should parse and remove optional device" do

            argv = [ "a device", "/mountpoint" , "-o", "rw,debug,default_permissions" ]
            result = RFuse.parse_options(argv)

            result[:device].should == "a device"
            result[:mountpoint].should == "/mountpoint"
            result[:rw].should be_true
            result[:debug].should be_true
            result[:default_permissions].should be_true

            argv.length.should == 3
            argv.join(" ").should == "/mountpoint -o rw,debug,default_permissions"
        end

    end
end
