const { killZombies, getActiveProcesses, setActiveProcesses } = require('./server');
const child_process = require('child_process');

jest.mock('child_process', () => ({
    spawn: jest.fn(),
    execSync: jest.fn()
}));

describe('killZombies', () => {
    beforeEach(() => {
        // Clear mocks and reset state before each test
        jest.clearAllMocks();
        setActiveProcesses([]);
    });

    it('should kill all active processes with SIGKILL and clear the activeProcesses array', () => {
        const mockProcess1 = { kill: jest.fn() };
        const mockProcess2 = { kill: jest.fn() };

        setActiveProcesses([mockProcess1, mockProcess2]);

        killZombies();

        expect(mockProcess1.kill).toHaveBeenCalledWith('SIGKILL');
        expect(mockProcess2.kill).toHaveBeenCalledWith('SIGKILL');
        expect(getActiveProcesses()).toEqual([]);
    });

    it('should forcefully execute Linux killall to guarantee ports are freed', () => {
        killZombies();

        expect(child_process.execSync).toHaveBeenCalledWith('killall -9 sat_a sat_b sat_c ground_station 2>/dev/null');
    });

    it('should handle exceptions gracefully when a process fails to be killed', () => {
        const mockProcess1 = { kill: jest.fn(() => { throw new Error('Failed to kill process'); }) };
        const mockProcess2 = { kill: jest.fn() };

        setActiveProcesses([mockProcess1, mockProcess2]);

        // Should not throw an error
        expect(() => killZombies()).not.toThrow();

        expect(mockProcess1.kill).toHaveBeenCalledWith('SIGKILL');
        expect(mockProcess2.kill).toHaveBeenCalledWith('SIGKILL');
        expect(getActiveProcesses()).toEqual([]);
        expect(child_process.execSync).toHaveBeenCalledWith('killall -9 sat_a sat_b sat_c ground_station 2>/dev/null');
    });

    it('should handle exceptions gracefully when execSync fails', () => {
        const mockProcess1 = { kill: jest.fn() };
        setActiveProcesses([mockProcess1]);

        child_process.execSync.mockImplementationOnce(() => {
            throw new Error('execSync failed');
        });

        // Should not throw an error
        expect(() => killZombies()).not.toThrow();

        expect(mockProcess1.kill).toHaveBeenCalledWith('SIGKILL');
        expect(getActiveProcesses()).toEqual([]);
        expect(child_process.execSync).toHaveBeenCalledWith('killall -9 sat_a sat_b sat_c ground_station 2>/dev/null');
    });
});
