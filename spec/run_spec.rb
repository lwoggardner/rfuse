require 'spec_helper'

describe RFuse::Fuse do
    context "#run" do

        let(:file_stat) { RFuse::Stat.file(0444) }
        let(:mountpoint) { tempmount() }
        let(:mockfs) { m = double("fuse"); allow(m).to receive(:getattr) { |ctx,path| puts "#{ctx},#{path}"}; m }
        let(:fuse) { RFuse::FuseDelegator.new(mockfs,mountpoint) }

        before(:each) do
            # Override traps (note rspec itself traps "INT") with "DEFAULT"
            @traps = ["INT","TERM","HUP","USR1","USR2"].inject({}) { |h,signame| h[signame] = Signal.trap(signame,"DEFAULT"); h }
        end

        after(:each) do
            # Restore previously set traps
            @traps.each_pair { |signame,prev_trap| Signal.trap(signame,prev_trap) }
        end

        it "runs a mounted filesystem with default traps" do

            # expect traps to be set
            expect(mockfs).to receive(:sighup) { }
            expect(mockfs).to receive(:getattr).with(anything(),"/run").and_return(file_stat)

            # Need to call this before we fork..
            expect(fuse.mountpoint).to eq(mountpoint)

            pid = Process.pid

            fpid = Kernel.fork do
                File.stat("#{mountpoint}/run")
                Process.kill("HUP",pid)
                sleep(0.1)
                Process.kill("TERM",pid)
            end

            fuse.run

            rpid,result = Process.waitpid2(fpid)
            expect(result).to be_success

            expect(fuse).not_to be_mounted
            expect(Signal.trap("HUP","DEFAULT")).to eq("DEFAULT")
        end

        it "unmounts the filesystem and resets traps on exception" do
            expect(mockfs).to receive(:getattr).with(anything(),"/run").and_return(file_stat)
            # raising an error in a sighandler will exit the loop..
            expect(mockfs).to receive(:sighup).and_raise("oh noes")
            pid = Process.pid

            # Need to call this before we fork..
            expect(fuse.mountpoint).to eq(mountpoint)


            fpid = Kernel.fork do
                File.stat("#{mountpoint}/run")
                Process.kill("HUP",pid)
            end

            fuse.run

            pid,result = Process.waitpid2(fpid)

            expect(fuse).not_to be_mounted
            expect(Signal.trap("HUP","DEFAULT")).to eq("DEFAULT")
        end

    end
end
