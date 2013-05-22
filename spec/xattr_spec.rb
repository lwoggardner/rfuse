require 'spec_helper'
require 'ffi-xattr'

describe RFuse::Fuse do
        it "should handle extended attributes" do
            pending "broken on mac" if mac?

            mockfs = mock("fuse")

            file_stat = RFuse::Stat.file(0444,:size => 11)

            mockfs.stub(:getattr).and_return(file_stat)
            mockfs.stub(:getxattr) { |ctx,path,name|
                case name
                when "user.one"
                    "1"
                when "user.two"
                    "2"
                end
            }

            mockfs.stub(:listxattr).with(anything(),"/myfile").and_return([ "user.one","user.two" ])
            mockfs.should_receive(:setxattr).with(anything(),"/myfile","user.three","updated",anything())
            mockfs.should_receive(:removexattr).with(anything(),"/myfile","user.one")

            mountpoint = tempmount()

            with_fuse(mountpoint,mockfs) do
                xattr = Xattr.new("#{mountpoint}/myfile")
                xattr.list.should include("user.one")
                xattr.list.should include("user.two")
                xattr.list.size.should == 2

                xattr['user.one'].should == "1"
                xattr['user.two'].should == "2"
                xattr['user.three']= "updated"
                xattr.remove('user.one')
            end
        end

end
