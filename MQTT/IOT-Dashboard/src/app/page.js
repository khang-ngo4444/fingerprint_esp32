"use client";
import React, { useState } from 'react';
import { Eye, EyeOff, User, Lock } from 'lucide-react';
import { useRouter } from 'next/navigation';

export default function LoginHomepage() {
  const [showPassword, setShowPassword] = useState(false);
  const [accountName, setAccountName] = useState('');
  const [password, setPassword] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');
  const router = useRouter();

  const handleSubmit = (e) => {
    e.preventDefault();
    setIsLoading(true);
    setError('');

    // Hardcoded admin credentials
    if (accountName === 'admin' && password === '123qwe!@#') {
      setTimeout(() => {
        setIsLoading(false);
        router.push('/dashboard_admin');
      }, 500);
    } else {
      setTimeout(() => {
        setIsLoading(false);
        setError('Invalid account name or password.');
      }, 500);
    }
  };

  return (
    <div className="min-h-screen bg-slate-900 flex items-center justify-center p-4">
      <div className="w-full max-w-md">
        
        {/* Simplified Logo Section */}
        <div className="text-center mb-8">
          <div className="inline-flex items-center justify-center w-16 h-16 bg-blue-600 rounded-2xl mb-4">
            {/* You can place a static SVG or Icon here */}
            <User className="w-8 h-8 text-white" />
          </div>
          <h1 className="text-4xl font-bold text-slate-50 mb-2">
            Welcome Back
          </h1>
          <p className="text-slate-400 text-lg">
            Sign in to continue your journey
          </p>
        </div>

        {/* Simplified Login Form */}
        <div className="bg-slate-800 rounded-2xl p-8 border border-slate-700">
          <form onSubmit={handleSubmit} className="space-y-6">
            
            {/* Account Name Field */}
            <div>
              <label className="block text-sm font-semibold text-slate-300 mb-2">
                Account Name
              </label>
              <div className="relative">
                <User className="absolute left-3 top-1/2 transform -translate-y-1/2 text-slate-500 w-5 h-5" />
                <input
                  type="text"
                  value={accountName}
                  onChange={(e) => setAccountName(e.target.value)}
                  className="w-full pl-10 pr-4 py-3 border border-slate-600 bg-slate-700 text-slate-200 rounded-lg focus:ring-2 focus:ring-blue-500 focus:outline-none transition-colors"
                  placeholder="Enter your account name"
                  required
                />
              </div>
            </div>

            {/* Password Field */}
            <div>
              <label className="block text-sm font-semibold text-slate-300 mb-2">
                Password
              </label>
              <div className="relative">
                <Lock className="absolute left-3 top-1/2 transform -translate-y-1/2 text-slate-500 w-5 h-5" />
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  className="w-full pl-10 pr-12 py-3 border border-slate-600 bg-slate-700 text-slate-200 rounded-lg focus:ring-2 focus:ring-blue-500 focus:outline-none transition-colors"
                  placeholder="Enter your password"
                  required
                />
                <button
                  type="button"
                  onClick={() => setShowPassword(!showPassword)}
                  className="absolute right-3 top-1/2 transform -translate-y-1/2 text-slate-400 hover:text-slate-200 transition-colors"
                >
                  {showPassword ? <EyeOff className="w-5 h-5" /> : <Eye className="w-5 h-5" />}
                </button>
              </div>
            </div>

            {/* Options Row */}
            <div className="flex items-center justify-between">
              <label className="flex items-center cursor-pointer">
                <input
                  type="checkbox"
                  className="w-4 h-4 text-blue-600 bg-slate-700 border-slate-600 rounded focus:ring-blue-500"
                />
                <span className="ml-2 text-sm text-slate-400">
                  Remember me
                </span>
              </label>
              <button
                type="button"
                className="text-sm text-blue-500 hover:text-blue-400 font-semibold transition-colors"
              >
                Forgot password?
              </button>
            </div>

            {/* Error Message */}
            {error && (
              <div className="text-red-500 text-sm text-center">{error}</div>
            )}

            {/* Submit Button */}
            <button
              type="submit"
              disabled={isLoading}
              className="w-full bg-blue-600 text-white py-3 px-6 rounded-lg font-semibold hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-offset-slate-900 focus:ring-blue-500 transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center"
            >
              {isLoading ? (
                <>
                  <div className="w-5 h-5 border-2 border-white/50 border-t-white rounded-full animate-spin mr-2" />
                  <span>Signing in...</span>
                </>
              ) : (
                'Sign In'
              )}
            </button>
          </form>
        </div>

        {/* Footer */}
        <div className="mt-8 text-center text-sm text-slate-500">
          <p>
            By signing in, you agree to our{' '}
            <button className="text-blue-500 hover:text-blue-400 font-medium">
              Terms of Service
            </button>{' '}
            and{' '}
            <button className="text-blue-500 hover:text-blue-400 font-medium">
              Privacy Policy
            </button>
          </p>
        </div>
      </div>
    </div>
  );
}