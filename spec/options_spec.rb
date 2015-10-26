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
            fuse.mounted?.should be_falsey
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should behave sensibly for bad mountpoint" do
            fuse = RFuse::FuseDelegator.new(mockfs,"bad/mount/point")
            fuse.mounted?.should be_falsey
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should behave sensibly for bad options" do
            fuse = RFuse::FuseDelegator.new(mockfs,mountpoint,"-eviloption")
            fuse.mounted?.should be_falsey
            lambda { fuse.loop }.should raise_error(RFuse::Error)
        end

        it "should handle a Pathname as a mountpoint" do
            fuse = RFuse::FuseDelegator.new(mockfs,Pathname.new(mountpoint))
            fuse.mounted?.should be(true)
            fuse.unmount()
        end
    end

    context "#parse_options" do

        it "should detect -h" do
            argv = [ "/mountpoint","-h" ]
            result = RFuse.parse_options(argv)

            result[:help].should be(true)
            result[:mountpoint].should == "/mountpoint"
            result[:debug].should be_falsey
        end

        it "should detect -h mixed with -o" do
            argv = [ "/mountpoint","-h", "-o", "debug" ]
            result = RFuse.parse_options(argv)

            result[:help].should be(true)
            result[:mountpoint].should == "/mountpoint"
            result[:debug].should be(true)
        end

        it "should detect debug" do
            argv = [ "/mountpoint","-o","debug" ]
            result = RFuse.parse_options(argv)

            result[:debug].should be(true)
            result[:help].should be_falsey

            argv = [ "/mountpoint","-o","default_permissions,debug" ]
            result = RFuse.parse_options(argv)
            result[:debug].should be(true)
        end

        it "detects debug as -d" do
            argv = [ "/mountpoint","-o","someopt","-d" ]
            result = RFuse.parse_options(argv)
            result[:debug].should be(true)
        end

        it "should remove local options" do
            argv = [ "/mountpoint","-o","debug,myoption" ]

            result = RFuse.parse_options(argv,:myoption)

            result[:debug].should be(true)
            result[:myoption].should be(true)

            argv.should == [ "/mountpoint", "-o", "debug" ]
        end

        it "should remove the only local option" do
            argv = ["/mountpoint","-o","myoption" ]

            result = RFuse.parse_options(argv,:myoption)

            result[:myoption].should be(true)
            argv.should == [ "/mountpoint" ]
        end


        it "parses the value from the only local option" do
            argv = [ "adevice", "/mountpoint", "-o","someopt=somevalue"]
            result = RFuse.parse_options(argv,:someopt)

            result[:someopt].should == "somevalue"
            argv.should == [ "/mountpoint" ]
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
            result[:rw].should be(true)
            result[:debug].should be(true)
            result[:default_permissions].should be(true)

            argv.should == [ "/mountpoint" , "-o", "rw,debug,default_permissions" ]
        end

        context "with '-' in device" do
            it "should parse the device correctly" do
                argv = [ "a-device", "/mountpoint" , "-o", "rw,debug,default_permissions" ]
                result = RFuse.parse_options(argv)

                result[:device].should == "a-device"
            end
        end
    end
end
