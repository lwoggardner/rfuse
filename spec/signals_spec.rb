require 'spec_helper'

describe RFuse::Fuse do

    let(:mountpoint) { tempmount() }
    let(:fuse) { RFuse::Fuse.new(mountpoint) }

    before(:each) do
        # Override traps (note rspec itself traps "INT") with "DEFAULT"
        @traps = ["INT","TERM","HUP","USR1","USR2"].inject({}) { |h,signame| h[signame] = Signal.trap(signame,"DEFAULT"); h }
    end

    after(:each) do
        fuse.unmount()
        # Restore previously set traps
        @traps.each_pair { |signame,prev_trap| Signal.trap(signame,prev_trap) }
    end

    context "#trap_signals" do

        it "returns the list of signals trapped" do

            allow(fuse).to receive(:sighup)

            expect(fuse.trap_signals("HUP")).to include("HUP")
            expect(Signal.trap("HUP","DEFAULT")).not_to eq("DEFAULT")
        end

        it "does not override previously set traps" do

            allow(fuse).to receive(:sighup)

            Signal.trap("HUP","HUPCOMMAND")

            expect(fuse.trap_signals()).not_to include("HUP")
            expect(Signal.trap("HUP","DEFAULT")).to eq("HUPCOMMAND")
        end

        it "only traps the specified signals" do

            allow(fuse).to receive(:sighup)
            allow(fuse).to receive(:sigusr2)

            expect(fuse.trap_signals("HUP")).to include("HUP")
            expect(Signal.trap("HUP","DEFAULT")).not_to eq("DEFAULT")
            expect(Signal.trap("USR2","DEFAULT")).to eq("DEFAULT")
        end

        it "allows signals to be handled by the filesystem" do
            expect(fuse).to receive(:sighup) { fuse.exit }

            expect(fuse.trap_signals("HUP")).to include("HUP")

            pid = Process.pid
            fork_fuse(fuse) { Process.kill("HUP",pid) }
            # Exitted loop, but still mounted - ie not with fusermount -u
            expect(fuse).to be_mounted
        end

        it "exits on INT by default" do
            pid = Process.pid
            expect(fuse.trap_signals("INT")).to include("INT")

            fork_fuse(fuse) { Process.kill("INT",pid) }
            expect(fuse).to be_mounted
        end

        it "exits on TERM by default" do
            pid = Process.pid
            expect(fuse.trap_signals("TERM")).to include("TERM")

            fork_fuse(fuse) { Process.kill("TERM",pid) }
            expect(fuse).to be_mounted
        end

        context "with a delegated filesystem" do
            let(:mockfs) { m = mock("fuse"); m.stub(:getattr).and_return(nil); m }
            let(:fuse) { RFuse::FuseDelegator.new(mockfs,mountpoint) }

            it "enables FuseDelegator debugging on USR1" do

                pid = Process.pid
                expect(fuse.trap_signals("USR1","INT")).to include("USR1")

                expect(fuse.debug?).to be(false)

                fork_fuse(fuse) do
                    Process.kill("USR1",pid)
                    sleep(0.1)
                    Process.kill("INT",pid)
                end

                expect(fuse.debug?).to be(true)
            end

            it "allows delegate filesystem to override default sig handler method" do
                expect(mockfs).to receive(:sigint) { fuse.exit }

                pid = Process.pid
                fuse.trap_signals("INT")

                fork_fuse(fuse) do
                    Process.kill("INT",pid)
                end
            end
        end
    end
end
